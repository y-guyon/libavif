// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <fstream>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace libavif {
namespace {

TEST(RangeTest, DifferentVideoRangeInColrAndMdat) {
  for (bool use_grid : {false, true}) {
    testutil::AvifImagePtr image = testutil::CreateImage(
        /*width=*/64, /*height=*/64, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV420,
        AVIF_PLANES_ALL, AVIF_RANGE_LIMITED);
    ASSERT_NE(image, nullptr);
    testutil::FillImageGradient(image.get());  // Pixel values do not matter.

    // Generate a valid AVIF stream with the video range flag set to limited in
    // the "colr" box and in the AV1 payload (in the "mdat" box).
    testutil::AvifEncoderPtr encoder(avifEncoderCreate(), avifEncoderDestroy);
    ASSERT_NE(encoder, nullptr);
    testutil::AvifRwData encoded;
    if (use_grid) {
      const avifImage* cellImages[2] = {image.get(), image.get()};
      ASSERT_EQ(
          avifEncoderAddImageGrid(encoder.get(), /*gridCols=*/2, /*gridRows=*/1,
                                  cellImages, AVIF_ADD_IMAGE_FLAG_SINGLE),
          AVIF_RESULT_OK);
      ASSERT_EQ(avifEncoderFinish(encoder.get(), &encoded), AVIF_RESULT_OK);
    } else {
      ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
                AVIF_RESULT_OK);
    }

    // Set full_range_flag to 1 in the "colr" box only.
    // This creates an invalid bitstream according to AV1-ISOBMFF v1.2.0.
    const uint8_t kColrBoxTag[] = "colr";
    uint8_t* colr_box = std::search(encoded.data, encoded.data + encoded.size,
                                    kColrBoxTag, kColrBoxTag + 4);
    ASSERT_GT(colr_box, encoded.data + 4);
    ASSERT_LT(colr_box, encoded.data + encoded.size);
    const uint32_t colr_box_size = (colr_box[-4] << 24) | (colr_box[-3] << 16) |
                                   (colr_box[-2] << 8) | colr_box[-1];
    ASSERT_EQ(colr_box_size, 19u);
    ASSERT_LT(colr_box + colr_box_size - 4, encoded.data + encoded.size);
    ASSERT_TRUE(std::equal(colr_box + 4, colr_box + 8,
                           reinterpret_cast<const uint8_t*>("nclx")));
    colr_box[colr_box_size - 5] = 1 << 7;  // full_range_flag=1, reserved=0.

    // Make sure the now invalid bitstream is refused.
    testutil::AvifImagePtr decoded(avifImageCreateEmpty(), avifImageDestroy);
    ASSERT_NE(decoded, nullptr);
    testutil::AvifDecoderPtr decoder(avifDecoderCreate(), avifDecoderDestroy);
    ASSERT_NE(decoder, nullptr);
    ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                    encoded.size),
              AVIF_RESULT_BMFF_PARSE_FAILED);
  }
}

}  // namespace
}  // namespace libavif
