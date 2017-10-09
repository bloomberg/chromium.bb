// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/android_video_decode_accelerator.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/android_video_decode_accelerator.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/avda_codec_allocator.h"
#include "media/gpu/android/fake_android_video_surface_chooser.h"
#include "media/gpu/android/fake_codec_allocator.h"
#include "media/gpu/android/mock_device_info.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::_;

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

class MockVDAClient : public VideoDecodeAccelerator::Client {
 public:
  MockVDAClient() {}

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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVDAClient);
};

}  // namespace

class AndroidVideoDecodeAcceleratorTest : public testing::Test {
 public:
  // Default to baseline H264 because it's always supported.
  AndroidVideoDecodeAcceleratorTest()
      : gl_decoder_(&command_buffer_service_, &outputter_),
        config_(H264PROFILE_BASELINE) {}

  void SetUp() override {
    ASSERT_TRUE(gl::init::InitializeGLOneOff());
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size(16, 16));
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    context_->MakeCurrent(surface_.get());

    codec_allocator_ = base::MakeUnique<FakeCodecAllocator>(
        base::SequencedTaskRunnerHandle::Get());
    device_info_ = base::MakeUnique<NiceMock<MockDeviceInfo>>();

    chooser_that_is_usually_null_ =
        base::MakeUnique<NiceMock<FakeSurfaceChooser>>();
    chooser_ = chooser_that_is_usually_null_.get();

    // By default, allow deferred init.
    config_.is_deferred_initialization_allowed = true;
  }

  ~AndroidVideoDecodeAcceleratorTest() override {
    // ~AVDASurfaceBundle() might rely on GL being available, so we have to
    // explicitly drop references to them before tearing down GL.
    vda_ = nullptr;
    codec_allocator_ = nullptr;
    context_ = nullptr;
    surface_ = nullptr;
    gl::init::ShutdownGL();
  }

  // Create and initialize AVDA with |config_|, and return the result.
  bool InitializeAVDA(bool force_defer_surface_creation = false) {
    // Because VDA has a custom deleter, we must assign it to |vda_| carefully.
    AndroidVideoDecodeAccelerator* avda = new AndroidVideoDecodeAccelerator(
        codec_allocator_.get(), std::move(chooser_that_is_usually_null_),
        base::Bind(&MakeContextCurrent),
        base::Bind(&GetGLES2Decoder, gl_decoder_.AsWeakPtr()),
        AndroidOverlayMojoFactoryCB(), device_info_.get());
    vda_.reset(avda);
    avda->force_defer_surface_creation_for_testing_ =
        force_defer_surface_creation;

    bool result = vda_->Initialize(config_, &client_);
    base::RunLoop().RunUntilIdle();
    return result;
  }

  // Initialize |vda_|, providing a new surface for it.  You may get the surface
  // by asking |codec_allocator_|.
  void InitializeAVDAWithOverlay() {
    config_.overlay_info.surface_id = 123;
    ASSERT_TRUE(InitializeAVDA());
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(chooser_->factory_);

    // Have the factory provide an overlay, and verify that codec creation is
    // provided with that overlay.
    std::unique_ptr<MockAndroidOverlay> overlay =
        base::MakeUnique<MockAndroidOverlay>();
    overlay_callbacks_ = overlay->GetCallbacks();

    // Set the expectations first, since ProvideOverlay might cause callbacks.
    EXPECT_CALL(*codec_allocator_,
                MockCreateMediaCodecAsync(overlay.get(), nullptr));
    chooser_->ProvideOverlay(std::move(overlay));

    // Provide the codec so that we can check if it's freed properly.
    EXPECT_CALL(client_, NotifyInitializationComplete(true));
    codec_allocator_->ProvideMockCodecAsync();
    base::RunLoop().RunUntilIdle();
  }

  void InitializeAVDAWithSurfaceTexture() {
    ASSERT_TRUE(InitializeAVDA());
    base::RunLoop().RunUntilIdle();
    // We do not expect a factory, since we are using SurfaceTexture.
    ASSERT_FALSE(chooser_->factory_);

    // Set the expectations first, since ProvideOverlay might cause callbacks.
    EXPECT_CALL(*codec_allocator_,
                MockCreateMediaCodecAsync(nullptr, NotNull()));
    chooser_->ProvideSurfaceTexture();

    // Provide the codec so that we can check if it's freed properly.
    EXPECT_CALL(client_, NotifyInitializationComplete(true));
    codec_allocator_->ProvideMockCodecAsync();
    base::RunLoop().RunUntilIdle();
  }

  // Set whether HasUnrendereredPictureBuffers will return true or false.
  // TODO(liberato): We can't actually do this yet.  It turns out to be okay,
  // because AVDA doesn't actually SetSurface before DequeueOutput.  It could do
  // so, though, if there aren't unrendered buffers.  Should AVDA ever start
  // switching surfaces immediately upon receiving them, rather than waiting for
  // DequeueOutput, then we'll want to be able to indicate that it has
  // unrendered pictures to prevent that behavior.
  void SetHasUnrenderedPictureBuffers(bool flag) {}

  // Tell |avda_| to switch surfaces to its incoming surface.  This is a method
  // since we're a friend of AVDA, and the tests are subclasses.  It's also
  // somewhat hacky, but much less hacky than trying to run it via a timer.
  void LetAVDAUpdateSurface() {
    SetHasUnrenderedPictureBuffers(false);
    avda()->DequeueOutput();
  }

  // So that SequencedTaskRunnerHandle::Get() works.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  gpu::FakeCommandBufferServiceBase command_buffer_service_;
  gpu::gles2::TraceOutputter outputter_;
  NiceMock<gpu::gles2::MockGLES2Decoder> gl_decoder_;
  NiceMock<MockVDAClient> client_;
  std::unique_ptr<FakeCodecAllocator> codec_allocator_;

  // Only set until InitializeAVDA() is called.
  std::unique_ptr<FakeSurfaceChooser> chooser_that_is_usually_null_;
  FakeSurfaceChooser* chooser_;
  VideoDecodeAccelerator::Config config_;
  std::unique_ptr<MockDeviceInfo> device_info_;

  // Set by InitializeAVDAWithOverlay()
  MockAndroidOverlay::Callbacks overlay_callbacks_;

  // This must be a unique pointer to a VDA, not an AVDA, to ensure the
  // the default_delete specialization that calls Destroy() will be used.
  std::unique_ptr<VideoDecodeAccelerator> vda_;

  AndroidVideoDecodeAccelerator* avda() {
    return reinterpret_cast<AndroidVideoDecodeAccelerator*>(vda_.get());
  }
};

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureUnsupportedCodec) {
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  config_ = VideoDecodeAccelerator::Config(VIDEO_CODEC_PROFILE_UNKNOWN);
  ASSERT_FALSE(InitializeAVDA());
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       ConfigureSupportedCodecSynchronously) {
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  config_.is_deferred_initialization_allowed = false;

  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecSync(_, _));
  // AVDA must set client callbacks even in sync mode, so that the chooser is
  // in a sane state.  crbug.com/772899 .
  EXPECT_CALL(*chooser_, MockSetClientCallbacks());
  ASSERT_TRUE(InitializeAVDA());
  testing::Mock::VerifyAndClearExpectations(chooser_);
}

TEST_F(AndroidVideoDecodeAcceleratorTest, FailingToCreateACodecSyncIsAnError) {
  // Failuew to create a codec during sync init should cause Initialize to fail.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  config_.is_deferred_initialization_allowed = false;
  codec_allocator_->allow_sync_creation = false;

  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecSync(nullptr, NotNull()));
  ASSERT_FALSE(InitializeAVDA());
}

TEST_F(AndroidVideoDecodeAcceleratorTest, FailingToCreateACodecAsyncIsAnError) {
  // Verify that a null codec signals error for async init when it doesn't get a
  // mediacodec instance.
  //
  // Also assert that there's only one call to CreateMediaCodecAsync. And since
  // it replies with a null codec, AVDA will be in an error state when it shuts
  // down.  Since we know that it's constructed before we destroy the VDA, we
  // verify that AVDA doens't create codecs during destruction.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  // Note that if we somehow end up deferring surface creation, then this would
  // no longer be expected to fail.  It would signal success before asking for a
  // surface or codec.
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, NotNull()));
  EXPECT_CALL(client_, NotifyInitializationComplete(false));

  ASSERT_TRUE(InitializeAVDA());
  chooser_->ProvideSurfaceTexture();
  codec_allocator_->ProvideNullCodecAsync();

  // Make sure that codec allocation has happened before destroying the VDA.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       LowEndDevicesSucceedInitWithoutASurface) {
  // If AVDA decides that we should defer surface creation, then it should
  // signal success before we provide a surface.  It should still ask for a
  // surface, though.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  config_.overlay_info.surface_id = SurfaceManager::kNoSurfaceID;

  EXPECT_CALL(*chooser_, MockUpdateState()).Times(0);
  EXPECT_CALL(client_, NotifyInitializationComplete(true));

  // It would be nicer if we didn't just force this on, since we might do so
  // in a state that AVDA isn't supposed to handle (e.g., if we give it a
  // surface, then it would never decide to defer surface creation).
  bool force_defer_surface_creation = true;
  InitializeAVDA(force_defer_surface_creation);
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       AsyncInitWithSurfaceTextureAndDelete) {
  // When configuring with a SurfaceTexture and deferred init, we should be
  // asked for a codec, and be notified of init success if we provide one. When
  // AVDA is destroyed, it should release the codec and surface texture.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithSurfaceTexture();

  // Delete the VDA, and make sure that it tries to free the codec and the right
  // surface texture.
  EXPECT_CALL(
      *codec_allocator_,
      MockReleaseMediaCodec(codec_allocator_->most_recent_codec,
                            codec_allocator_->most_recent_overlay,
                            codec_allocator_->most_recent_surface_texture));
  codec_allocator_->most_recent_codec_destruction_observer->ExpectDestruction();
  vda_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(AndroidVideoDecodeAcceleratorTest, AsyncInitWithSurfaceAndDelete) {
  // When |config_| specifies a surface, we should be given a factory during
  // startup for it.  When |chooser_| provides an overlay, the codec should be
  // allocated using it.  Shutdown should provide the overlay when releasing the
  // media codec.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithOverlay();

  // Delete the VDA, and make sure that it tries to free the codec and the
  // overlay that it provided to us.
  EXPECT_CALL(
      *codec_allocator_,
      MockReleaseMediaCodec(codec_allocator_->most_recent_codec,
                            codec_allocator_->most_recent_overlay,
                            codec_allocator_->most_recent_surface_texture));
  codec_allocator_->most_recent_codec_destruction_observer->ExpectDestruction();
  vda_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       SwitchesToSurfaceTextureWhenSurfaceDestroyed) {
  // Provide a surface, and a codec, then destroy the surface.  AVDA should use
  // SetSurface to switch to SurfaceTexture.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithOverlay();

  // It would be nice if we knew that this was a surface texture.  As it is, we
  // just destroy the VDA and expect that we're provided with one.  Hopefully,
  // AVDA is actually calling SetSurface properly.
  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_))
      .WillOnce(Return(true));
  codec_allocator_->most_recent_codec_destruction_observer
      ->VerifyAndClearExpectations();
  overlay_callbacks_.SurfaceDestroyed.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*codec_allocator_,
              MockReleaseMediaCodec(codec_allocator_->most_recent_codec,
                                    nullptr, NotNull()));
  vda_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(AndroidVideoDecodeAcceleratorTest, SwitchesToSurfaceTextureEventually) {
  // Provide a surface, and a codec, then request that AVDA switches to a
  // surface texture.  Verify that it does.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithOverlay();

  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_))
      .WillOnce(Return(true));

  // Note that it's okay if |avda_| switches before ProvideSurfaceTexture
  // returns, since it has no queued output anyway.
  chooser_->ProvideSurfaceTexture();
  LetAVDAUpdateSurface();

  // Verify that we're now using some surface texture.
  EXPECT_CALL(*codec_allocator_,
              MockReleaseMediaCodec(codec_allocator_->most_recent_codec,
                                    nullptr, NotNull()));
  codec_allocator_->most_recent_codec_destruction_observer->ExpectDestruction();
  vda_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       SetSurfaceFailureDoesntSwitchSurfaces) {
  // Initialize AVDA with a surface, then request that AVDA switches to a
  // surface texture.  When it tries to UpdateSurface, pretend to fail.  AVDA
  // should notify error, and also release the original surface.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithOverlay();

  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_))
      .WillOnce(Return(false));
  EXPECT_CALL(client_,
              NotifyError(AndroidVideoDecodeAccelerator::PLATFORM_FAILURE))
      .Times(1);
  codec_allocator_->most_recent_codec_destruction_observer
      ->VerifyAndClearExpectations();
  chooser_->ProvideSurfaceTexture();
  LetAVDAUpdateSurface();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       SwitchToSurfaceAndBackBeforeSetSurface) {
  // Ask AVDA to switch from ST to overlay, then back to ST before it has a
  // chance to do the first switch.  It should simply drop the overlay.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithSurfaceTexture();

  // Don't let AVDA switch immediately, else it could choose to SetSurface when
  // it first gets the overlay.
  SetHasUnrenderedPictureBuffers(true);
  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_)).Times(0);
  std::unique_ptr<MockAndroidOverlay> overlay =
      base::MakeUnique<MockAndroidOverlay>();
  // Make sure that the overlay is not destroyed too soon.
  std::unique_ptr<DestructionObserver> observer =
      overlay->CreateDestructionObserver();
  observer->DoNotAllowDestruction();

  chooser_->ProvideOverlay(std::move(overlay));

  // Now it is expected to drop the overlay.
  observer->ExpectDestruction();

  // While the incoming surface is pending, switch back to SurfaceTexture.
  chooser_->ProvideSurfaceTexture();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       ChangingOutputSurfaceVoluntarilyWithoutSetSurfaceIsIgnored) {
  // If we ask AVDA to change to SurfaceTexture should be ignored on platforms
  // that don't support SetSurface (pre-M or blacklisted).  It should also
  // ignore SurfaceTexture => overlay, but we don't check that.
  //
  // Also note that there are other probably reasonable things to do (like
  // signal an error), but we want to be sure that it doesn't try to SetSurface.
  // We also want to be sure that, if it doesn't signal an error, that it also
  // doesn't get confused about which surface is in use.  So, we assume that it
  // doesn't signal an error, and we check that it releases the right surface
  // with the codec.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();
  EXPECT_CALL(client_, NotifyError(_)).Times(0);

  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  InitializeAVDAWithOverlay();
  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_)).Times(0);

  // This should not switch to SurfaceTexture.
  chooser_->ProvideSurfaceTexture();
  LetAVDAUpdateSurface();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       OnSurfaceDestroyedWithoutSetSurfaceFreesTheCodec) {
  // If AVDA receives OnSurfaceDestroyed without support for SetSurface, then it
  // should free the codec.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  InitializeAVDAWithOverlay();
  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_)).Times(0);

  // This should free the codec.
  EXPECT_CALL(
      *codec_allocator_,
      MockReleaseMediaCodec(codec_allocator_->most_recent_codec,
                            codec_allocator_->most_recent_overlay, nullptr));
  codec_allocator_->most_recent_codec_destruction_observer->ExpectDestruction();
  overlay_callbacks_.SurfaceDestroyed.Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the codec has been released, since |vda_| will be destroyed
  // soon.  The expectations must be met before that.
  testing::Mock::VerifyAndClearExpectations(&codec_allocator_);
  codec_allocator_->most_recent_codec_destruction_observer
      ->VerifyAndClearExpectations();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       MultipleSurfaceTextureCallbacksAreIgnored) {
  // Ask AVDA to switch to ST when it's already using ST, nothing should happen.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();

  InitializeAVDAWithSurfaceTexture();

  // This should do nothing.
  EXPECT_CALL(*codec_allocator_->most_recent_codec, SetSurface(_)).Times(0);
  chooser_->ProvideSurfaceTexture();

  base::RunLoop().RunUntilIdle();
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       OverlayInfoWithDuplicateSurfaceIDDoesntChangeTheFactory) {
  // Send OverlayInfo with duplicate info, and verify that it doesn't change
  // the factory.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();
  InitializeAVDAWithOverlay();

  EXPECT_CALL(*chooser_, MockUpdateState()).Times(1);
  EXPECT_CALL(*chooser_, MockReplaceOverlayFactory(_)).Times(0);
  OverlayInfo overlay_info = config_.overlay_info;
  avda()->SetOverlayInfo(overlay_info);
}

TEST_F(AndroidVideoDecodeAcceleratorTest,
       OverlayInfoWithNewSurfaceIDDoesChangeTheFactory) {
  // Send OverlayInfo with new surface info, and verify that it does change the
  // overlay factory.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();
  InitializeAVDAWithOverlay();

  EXPECT_CALL(*chooser_, MockUpdateState()).Times(1);
  OverlayInfo overlay_info = config_.overlay_info;
  overlay_info.surface_id++;
  avda()->SetOverlayInfo(overlay_info);
}

TEST_F(AndroidVideoDecodeAcceleratorTest, FullscreenSignalIsSentToChooser) {
  // Send OverlayInfo that has |is_fullscreen| set, and verify that the chooser
  // is notified about it.
  SKIP_IF_MEDIACODEC_IS_NOT_AVAILABLE();
  InitializeAVDAWithOverlay();
  OverlayInfo overlay_info = config_.overlay_info;
  overlay_info.is_fullscreen = !config_.overlay_info.is_fullscreen;
  avda()->SetOverlayInfo(overlay_info);
  ASSERT_EQ(chooser_->current_state_.is_fullscreen, overlay_info.is_fullscreen);
}

}  // namespace media
