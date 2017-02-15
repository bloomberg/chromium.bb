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
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_jni_registrar.h"
#include "media/gpu/android_video_decode_accelerator.h"
#include "media/gpu/avda_codec_allocator.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

using ::testing::_;
using ::testing::NiceMock;

namespace media {
namespace {

#define SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE()     \
  do {                                            \
    if (!MediaCodecUtil::IsMediaCodecAvailable()) \
      return;                                     \
  } while (false)

bool MakeContextCurrent() {
  return true;
}

base::WeakPtr<gpu::gles2::GLES2Decoder> GetGLES2Decoder(
    const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder) {
  return decoder;
}

ACTION(PostNullCodec) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&media::AVDACodecAllocatorClient::OnCodecConfigured,
                            arg0, nullptr));
}

}  // namespace

class MockVDAClient : public VideoDecodeAccelerator::Client {
 public:
  MOCK_METHOD1(NotifyInitializationComplete, void(bool));
  MOCK_METHOD5(
      ProvidePictureBuffers,
      void(uint32_t, VideoPixelFormat, uint32_t, const gfx::Size&, uint32_t));
  MOCK_METHOD1(DismissPictureBuffer, void(int32_t));
  MOCK_METHOD1(PictureReady, void(const Picture&));
  MOCK_METHOD1(NotifyEndOfBitstreamBuffer, void(int32_t));
  MOCK_METHOD0(NotifyFlushDone, void());
  MOCK_METHOD0(NotifyResetDone, void());
  MOCK_METHOD1(NotifyError, void(VideoDecodeAccelerator::Error));
};

class MockCodecAllocator : public AVDACodecAllocator {
 public:
  MOCK_METHOD2(CreateMediaCodecAsync,
               void(base::WeakPtr<AVDACodecAllocatorClient>,
                    scoped_refptr<CodecConfig>));
};

class AndroidVideoDecodeAcceleratorTest : public testing::Test {
 public:
  ~AndroidVideoDecodeAcceleratorTest() override {}

  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    RegisterJni(env);

    gl::init::ShutdownGL();
    ASSERT_TRUE(gl::init::InitializeGLOneOff());
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size(16, 16));
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    context_->MakeCurrent(surface_.get());

    vda_.reset(new AndroidVideoDecodeAccelerator(
        &codec_allocator_, base::Bind(&MakeContextCurrent),
        base::Bind(&GetGLES2Decoder, gl_decoder_.AsWeakPtr())));
  }

  base::MessageLoop message_loop_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  NiceMock<gpu::gles2::MockGLES2Decoder> gl_decoder_;
  NiceMock<MockVDAClient> client_;
  NiceMock<MockCodecAllocator> codec_allocator_;

  // This must be a unique pointer to a VDA, not an AVDA, to ensure the
  // the default_delete specialization that calls Destroy() will be used.
  std::unique_ptr<VideoDecodeAccelerator> vda_;
};

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureUnsupportedCodec) {
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  ASSERT_FALSE(vda_->Initialize(
      VideoDecodeAccelerator::Config(VIDEO_CODEC_PROFILE_UNKNOWN), &client_));
}

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureSupportedCodec) {
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  // H264 is always supported by AVDA.
  ASSERT_TRUE(vda_->Initialize(
      VideoDecodeAccelerator::Config(H264PROFILE_BASELINE), &client_));
}

TEST_F(AndroidVideoDecodeAcceleratorTest, FailingToCreateACodecIsAnError) {
  EXPECT_CALL(codec_allocator_, CreateMediaCodecAsync(_, _))
      .WillOnce(PostNullCodec());
  EXPECT_CALL(client_, NotifyInitializationComplete(false));

  VideoDecodeAccelerator::Config config(H264PROFILE_BASELINE);
  config.is_deferred_initialization_allowed = true;
  vda_->Initialize(config, &client_);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AndroidVideoDecodeAcceleratorTest, NoCodecsAreCreatedDuringDestruction) {
  // Assert that there's only one call to CreateMediaCodecAsync. And since it
  // replies with a null codec, AVDA will be in an error state when it shuts
  // down.
  EXPECT_CALL(codec_allocator_, CreateMediaCodecAsync(_, _))
      .WillOnce(PostNullCodec());

  VideoDecodeAccelerator::Config config(H264PROFILE_BASELINE);
  config.is_deferred_initialization_allowed = true;
  vda_->Initialize(config, &client_);
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
