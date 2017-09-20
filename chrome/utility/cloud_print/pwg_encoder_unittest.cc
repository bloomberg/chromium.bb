// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/cloud_print/pwg_encoder.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/sha1.h"
#include "chrome/utility/cloud_print/bitmap_image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cloud_print {

namespace {

// SHA-1 of golden master for this test, plus null terminating character.
// File is in chrome/test/data/printing/test_pwg_generator.pwg.
const char kPWGFileSha1[] = {'\x4a', '\xd7', '\x44', '\x29', '\x98', '\xc8',
                             '\xfe', '\xae', '\x94', '\xbc', '\x9c', '\x8b',
                             '\x17', '\x7a', '\x7c', '\x94', '\x76', '\x6c',
                             '\xc9', '\xfb', '\0'};

const int kRasterWidth = 612;
const int kRasterHeight = 792;
const int kRasterDPI = 72;

std::unique_ptr<BitmapImage> MakeSampleBitmap() {
  auto bitmap_image = std::make_unique<BitmapImage>(
      gfx::Size(kRasterWidth, kRasterHeight), BitmapImage::RGBA);
  uint32_t* bitmap_data =
      reinterpret_cast<uint32_t*>(bitmap_image->pixel_data());
  for (int i = 0; i < kRasterWidth * kRasterHeight; i++)
    bitmap_data[i] = 0xFFFFFF;

  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 200; j < 300; j++) {
      int row_start = j * kRasterWidth;
      uint32_t red = (i * 255) / kRasterWidth;
      bitmap_data[row_start + i] = red;
    }
  }

  // To test run length encoding
  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 400; j < 500; j++) {
      int row_start = j * kRasterWidth;
      if ((i/40) % 2 == 0) {
        bitmap_data[row_start + i] = 255 << 8;
      } else {
        bitmap_data[row_start + i] = 255 << 16;
      }
    }
  }

  return bitmap_image;
}

}  // namespace

TEST(PwgRasterTest, CompareWithMaster) {
  std::unique_ptr<BitmapImage> image = MakeSampleBitmap();
  PwgHeaderInfo header_info;
  header_info.dpi = kRasterDPI;
  header_info.total_pages = 1;

  std::string output = PwgEncoder::GetDocumentHeader();
  output += PwgEncoder::EncodePage(*image, header_info);
  EXPECT_EQ(kPWGFileSha1, base::SHA1HashString(output));
}

}  // namespace cloud_print
