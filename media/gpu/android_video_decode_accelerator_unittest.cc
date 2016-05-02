// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_decode_accelerator.h"

#include <stdint.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_jni_registrar.h"
#include "media/gpu/android_copying_backing_strategy.h"
#include "media/gpu/android_video_decode_accelerator.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace {

bool MockMakeContextCurrent() {
  return true;
}

static base::WeakPtr<gpu::gles2::GLES2Decoder> MockGetGLES2Decoder(
    const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder) {
  return decoder;
}

}  // namespace

namespace media {

class MockVideoDecodeAcceleratorClient
    : public media::VideoDecodeAccelerator::Client {
 public:
  MockVideoDecodeAcceleratorClient() {}
  ~MockVideoDecodeAcceleratorClient() override {}

  // VideoDecodeAccelerator::Client implementation.
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override {}
  void DismissPictureBuffer(int32_t picture_buffer_id) override {}
  void PictureReady(const media::Picture& picture) override {}
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override {}
  void NotifyFlushDone() override {}
  void NotifyResetDone() override {}
  void NotifyError(media::VideoDecodeAccelerator::Error error) override {}
};

class AndroidVideoDecodeAcceleratorTest : public testing::Test {
 public:
  ~AndroidVideoDecodeAcceleratorTest() override {}

 protected:
  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    media::RegisterJni(env);

    // Start message loop because
    // AndroidVideoDecodeAccelerator::ConfigureMediaCodec() starts a timer task.
    message_loop_.reset(new base::MessageLoop());

    std::unique_ptr<gpu::gles2::MockGLES2Decoder> decoder(
        new gpu::gles2::MockGLES2Decoder());
    std::unique_ptr<MockVideoDecodeAcceleratorClient> client(
        new MockVideoDecodeAcceleratorClient());
    accelerator_.reset(new AndroidVideoDecodeAccelerator(
        base::Bind(&MockMakeContextCurrent),
        base::Bind(&MockGetGLES2Decoder, decoder->AsWeakPtr())));
  }

  bool Configure(media::VideoCodec codec) {
    AndroidVideoDecodeAccelerator* accelerator =
        static_cast<AndroidVideoDecodeAccelerator*>(accelerator_.get());
    scoped_refptr<gfx::SurfaceTexture> surface_texture =
        gfx::SurfaceTexture::Create(0);
    accelerator->codec_config_->surface_ =
        gfx::ScopedJavaSurface(surface_texture.get());
    accelerator->codec_config_->codec_ = codec;
    return accelerator->ConfigureMediaCodecSynchronously();
  }

 private:
  std::unique_ptr<media::VideoDecodeAccelerator> accelerator_;
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureUnsupportedCodec) {
  EXPECT_FALSE(Configure(media::kUnknownVideoCodec));
}

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureSupportedCodec) {
  if (!media::MediaCodecUtil::IsMediaCodecAvailable())
    return;
  EXPECT_TRUE(Configure(media::kCodecVP8));
}

}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
