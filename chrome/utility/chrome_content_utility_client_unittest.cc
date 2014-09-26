// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_utility_messages.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "ipc/ipc_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace {

bool CreateJPEGImage(int width,
                     int height,
                     SkColor color,
                     std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);

  const int kQuality = 50;
  if (!gfx::JPEGCodec::Encode(
          static_cast<const unsigned char*>(bitmap.getPixels()),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          width,
          height,
          bitmap.rowBytes(),
          kQuality,
          output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }
  return true;
}

}  // namespace

typedef testing::Test ChromeContentUtilityClientTest;

// Test that DecodeImage() doesn't return image message > (max message size)
TEST_F(ChromeContentUtilityClientTest, DecodeImageSizeLimit) {
  // Using actual limit generates 14000 x 9400 images, which causes the test to
  // timeout.  We test with a smaller limit for efficiency.
  const size_t kTestMessageSize = IPC::Channel::kMaximumMessageSize / 1024;
  ChromeContentUtilityClient::set_max_ipc_message_size_for_test(
      kTestMessageSize);
  // Approx max height for 3:2 image that will fit in IPC message;
  // 1.5 for width/height ratio, 4 for bytes/pixel
  int max_height_for_msg = sqrt(kTestMessageSize / (1.5 * 4));
  int base_msg_size = sizeof(ChromeUtilityHostMsg_DecodeImage_Succeeded);

  // Sizes which should trigger dimension-halving 0, 1 and 2 times
  int heights[] = { max_height_for_msg - 10,
                    max_height_for_msg + 10,
                    2 * max_height_for_msg + 10 };
  int widths[] = { heights[0] * 3 / 2, heights[1] * 3 / 2, heights[2] * 3 / 2 };
  for (size_t i = 0; i < arraysize(heights); i++) {
    std::vector<unsigned char> jpg;
    CreateJPEGImage(widths[i], heights[i], SK_ColorRED, &jpg);
    SkBitmap bitmap = ChromeContentUtilityClient::DecodeImage(jpg, true);

    // Check that image has been shrunk appropriately
    EXPECT_LT(bitmap.computeSize64() + base_msg_size,
              static_cast<int64_t>(kTestMessageSize));
    // Android does its own image shrinking for memory conservation deeper in
    // the decode, so more specific tests here won't work.
#if !defined(OS_ANDROID)
    EXPECT_EQ(widths[i] >> i, bitmap.width());
    EXPECT_EQ(heights[i] >> i, bitmap.height());

    // Check that if resize not requested and image exceeds IPC size limit,
    // an empty image is returned
    if (heights[i] > max_height_for_msg) {
      SkBitmap empty_bmp = ChromeContentUtilityClient::DecodeImage(jpg, false);
      EXPECT_TRUE(empty_bmp.empty());
    }
#endif
  }
}
