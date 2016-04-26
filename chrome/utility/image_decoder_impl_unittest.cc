// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_decoder_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "ipc/ipc_channel.h"
#include "skia/public/type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace mojom {

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
          gfx::JPEGCodec::FORMAT_SkBitmap, width, height,
          static_cast<int>(bitmap.rowBytes()), kQuality, output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }
  return true;
}

class Request {
 public:
  explicit Request(ImageDecoderImpl* decoder) : decoder_(decoder) {}

  void DecodeImage(const std::vector<unsigned char>& image, bool shrink) {
    decoder_->DecodeImage(
        mojo::Array<uint8_t>::From(image), ImageCodec::DEFAULT,
        shrink, base::Bind(&Request::OnRequestDone, base::Unretained(this)));
  }

  const skia::mojom::BitmapPtr& bitmap() const { return bitmap_; }

 private:
  void OnRequestDone(skia::mojom::BitmapPtr result_image) {
    bitmap_ = std::move(result_image);
  }

  ImageDecoderImpl* decoder_;
  skia::mojom::BitmapPtr bitmap_;
};

class ImageDecoderImplTest : public testing::Test {
 protected:
  ~ImageDecoderImplTest() override {}

  base::MessageLoop message_loop_;
};

}  // namespace

// Test that DecodeImage() doesn't return image message > (max message size)
TEST_F(ImageDecoderImplTest, DecodeImageSizeLimit) {
  // Using actual limit generates 14000 x 9400 images, which causes the test to
  // timeout.  We test with a smaller limit for efficiency.
  const size_t kTestMessageSize = IPC::Channel::kMaximumMessageSize / 1024;

  ImageDecoderImpl decoder(kTestMessageSize);

  // Approx max height for 3:2 image that will fit in IPC message;
  // 1.5 for width/height ratio, 4 for bytes/pixel
  int max_height_for_msg = sqrt(kTestMessageSize / (1.5 * 4));
  int base_msg_size = sizeof(skia::mojom::Bitmap::Data_);

  // Sizes which should trigger dimension-halving 0, 1 and 2 times
  int heights[] = {max_height_for_msg - 10,
                   max_height_for_msg + 10,
                   2 * max_height_for_msg + 10};
  int widths[] = {heights[0] * 3 / 2, heights[1] * 3 / 2, heights[2] * 3 / 2};
  for (size_t i = 0; i < arraysize(heights); i++) {
    std::vector<unsigned char> jpg;
    ASSERT_TRUE(CreateJPEGImage(widths[i], heights[i], SK_ColorRED, &jpg));

    Request request(&decoder);
    request.DecodeImage(jpg, true);
    ASSERT_FALSE(request.bitmap().is_null());
    SkBitmap bitmap = request.bitmap().To<SkBitmap>();

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
      Request request(&decoder);
      request.DecodeImage(jpg, false);
      EXPECT_TRUE(request.bitmap().is_null());
    }
#endif
  }
}

TEST_F(ImageDecoderImplTest, DecodeImageFailed) {
  ImageDecoderImpl decoder(IPC::Channel::kMaximumMessageSize);

  // The "jpeg" is just some "random" data;
  const char kRandomData[] = "u gycfy7xdjkhfgui bdui ";
  std::vector<unsigned char> jpg(kRandomData,
                                 kRandomData + sizeof(kRandomData));

  Request request(&decoder);
  request.DecodeImage(jpg, false);
  EXPECT_TRUE(request.bitmap().is_null());
}

}  // namespace mojom
