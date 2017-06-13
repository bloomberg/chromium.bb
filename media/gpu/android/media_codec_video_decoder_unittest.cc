// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/media_codec_video_decoder.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_jni_registrar.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_helpers.h"
#include "media/gpu/android/fake_codec_allocator.h"
#include "media/gpu/android/mock_device_info.h"
#include "media/gpu/android_video_surface_chooser_impl.h"
#include "media/gpu/fake_android_video_surface_chooser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace media {
namespace {

void OutputCb(const scoped_refptr<VideoFrame>& frame) {}

gpu::GpuCommandBufferStub* GetStubCb() {
  return nullptr;
}

}  // namespace

class MediaCodecVideoDecoderTest : public testing::Test {
 public:
  MediaCodecVideoDecoderTest() = default;

  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    RegisterJni(env);

    // We set up GL so that we can create SurfaceTextures.
    // TODO(watk): Create a mock SurfaceTexture so we don't have to
    // do this.
    ASSERT_TRUE(gl::init::InitializeGLOneOff());
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size(16, 16));
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    context_->MakeCurrent(surface_.get());

    codec_allocator_ = base::MakeUnique<FakeCodecAllocator>();
    device_info_ = base::MakeUnique<NiceMock<MockDeviceInfo>>();
    auto surface_chooser = base::MakeUnique<NiceMock<FakeSurfaceChooser>>();
    surface_chooser_ = surface_chooser.get();
    mcvd_ = base::MakeUnique<MediaCodecVideoDecoder>(
        base::ThreadTaskRunnerHandle::Get(), base::Bind(&GetStubCb),
        device_info_.get(), codec_allocator_.get(), std::move(surface_chooser));
  }

  ~MediaCodecVideoDecoderTest() override {
    // ~AVDASurfaceBundle() might rely on GL being available, so we have to
    // explicitly drop references to them before tearing down GL.
    mcvd_ = nullptr;
    codec_allocator_ = nullptr;
    context_ = nullptr;
    surface_ = nullptr;
    gl::init::ShutdownGL();
  }

  bool Initialize(const VideoDecoderConfig& config) {
    bool result = false;
    auto init_cb = [](bool* result_out, bool result) { *result_out = result; };
    mcvd_->Initialize(config, false, nullptr, base::Bind(init_cb, &result),
                      base::Bind(&OutputCb));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  MockAndroidOverlay* InitializeWithOverlay() {
    Initialize(TestVideoConfig::NormalH264());
    mcvd_->Decode(nullptr, decode_cb_.Get());
    auto overlay_ptr = base::MakeUnique<MockAndroidOverlay>();
    auto* overlay = overlay_ptr.get();
    surface_chooser_->ProvideOverlay(std::move(overlay_ptr));
    return overlay;
  }

  void InitializeWithSurfaceTexture() {
    Initialize(TestVideoConfig::NormalH264());
    mcvd_->Decode(nullptr, decode_cb_.Get());
    surface_chooser_->ProvideSurfaceTexture();
  }

 protected:
  std::unique_ptr<MediaCodecVideoDecoder> mcvd_;
  std::unique_ptr<MockDeviceInfo> device_info_;
  std::unique_ptr<FakeCodecAllocator> codec_allocator_;
  FakeSurfaceChooser* surface_chooser_;
  base::MockCallback<VideoDecoder::DecodeCB> decode_cb_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(MediaCodecVideoDecoderTest, DestructBeforeInitWorks) {
  // Do nothing.
}

TEST_F(MediaCodecVideoDecoderTest, UnknownCodecIsRejected) {
  ASSERT_FALSE(Initialize(TestVideoConfig::Invalid()));
}

TEST_F(MediaCodecVideoDecoderTest, H264IsSupported) {
  // H264 is always supported by MCVD.
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));
}

TEST_F(MediaCodecVideoDecoderTest, SmallVp8IsRejected) {
  ASSERT_FALSE(Initialize(TestVideoConfig::Normal()));
}

TEST_F(MediaCodecVideoDecoderTest, InitializeDoesntInitSurfaceOrCodec) {
  EXPECT_CALL(*surface_chooser_, MockInitialize()).Times(0);
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, _)).Times(0);
  Initialize(TestVideoConfig::NormalH264());
}

TEST_F(MediaCodecVideoDecoderTest, FirstDecodeTriggersSurfaceChooserInit) {
  Initialize(TestVideoConfig::NormalH264());
  EXPECT_CALL(*surface_chooser_, MockInitialize());
  mcvd_->Decode(nullptr, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest, CodecIsCreatedAfterSurfaceChosen) {
  Initialize(TestVideoConfig::NormalH264());
  mcvd_->Decode(nullptr, decode_cb_.Get());
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, NotNull()));
  surface_chooser_->ProvideSurfaceTexture();
}

TEST_F(MediaCodecVideoDecoderTest, DecodeCbsAreRunOnError) {
  InitializeWithSurfaceTexture();
  mcvd_->Decode(nullptr, decode_cb_.Get());
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(2);
  // Failing to create a codec should put MCVD into an error state.
  codec_allocator_->ProvideNullCodecAsync();
}

TEST_F(MediaCodecVideoDecoderTest, AfterInitCompletesTheCodecIsPolled) {
  InitializeWithSurfaceTexture();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();
  // Run a RunLoop until the first time the codec is polled for an available
  // input buffer.
  base::RunLoop loop;
  EXPECT_CALL(*codec, DequeueInputBuffer(_, _))
      .WillOnce(InvokeWithoutArgs([&loop]() {
        loop.Quit();
        return MEDIA_CODEC_TRY_AGAIN_LATER;
      }));
  loop.Run();
}

TEST_F(MediaCodecVideoDecoderTest, CodecIsReleasedOnDestruction) {
  InitializeWithSurfaceTexture();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec, _, _));
}

TEST_F(MediaCodecVideoDecoderTest,
       SurfaceChooserNotInitializedWithOverlayFactory) {
  InitializeWithSurfaceTexture();
  // The surface chooser should not have an overlay factory because
  // SetOverlayInfo() was not called before it was initialized.
  ASSERT_FALSE(surface_chooser_->factory_);
}

TEST_F(MediaCodecVideoDecoderTest,
       SurfaceChooserInitializedWithOverlayFactory) {
  Initialize(TestVideoConfig::NormalH264());
  OverlayInfo info;
  info.surface_id = 123;
  mcvd_->SetOverlayInfo(info);
  mcvd_->Decode(nullptr, decode_cb_.Get());
  // The surface chooser should have an overlay factory because SetOverlayInfo()
  // was called before it was initialized.
  ASSERT_TRUE(surface_chooser_->factory_);
}

TEST_F(MediaCodecVideoDecoderTest, SetOverlayInfoIsValidBeforeInitialize) {
  OverlayInfo info;
  info.surface_id = 123;
  mcvd_->SetOverlayInfo(info);
  Initialize(TestVideoConfig::NormalH264());
  mcvd_->Decode(nullptr, decode_cb_.Get());
  ASSERT_TRUE(surface_chooser_->factory_);
}

TEST_F(MediaCodecVideoDecoderTest, SetOverlayInfoReplacesTheOverlayFactory) {
  InitializeWithOverlay();

  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory()).Times(2);
  OverlayInfo info;
  info.surface_id = 123;
  mcvd_->SetOverlayInfo(info);
  info.surface_id = 456;
  mcvd_->SetOverlayInfo(info);
}

TEST_F(MediaCodecVideoDecoderTest, DuplicateSetOverlayInfosAreIgnored) {
  InitializeWithOverlay();

  // The second SetOverlayInfo() should be ignored.
  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory()).Times(1);
  OverlayInfo info;
  info.surface_id = 123;
  mcvd_->SetOverlayInfo(info);
  mcvd_->SetOverlayInfo(info);
}

TEST_F(MediaCodecVideoDecoderTest, CodecIsCreatedWithChosenOverlay) {
  AndroidOverlay* overlay_passed_to_codec = nullptr;
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, _))
      .WillOnce(SaveArg<0>(&overlay_passed_to_codec));
  auto* overlay = InitializeWithOverlay();
  DCHECK_EQ(overlay, overlay_passed_to_codec);
}

TEST_F(MediaCodecVideoDecoderTest,
       SurfaceDestroyedBeforeCodecCreationDropsCodec) {
  auto* overlay = InitializeWithOverlay();
  overlay->OnSurfaceDestroyed();

  // The codec is dropped as soon as it's ready.
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(_, _, _));
  codec_allocator_->ProvideMockCodecAsync();
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_F(MediaCodecVideoDecoderTest, SurfaceDestroyedDoesSyncSurfaceTransition) {
  auto* overlay = InitializeWithOverlay();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // MCVD must synchronously switch the codec's surface (to surface
  // texture), and delete the overlay.
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  auto observer = overlay->CreateDestructionObserver();
  observer->ExpectDestruction();
  overlay->OnSurfaceDestroyed();
}

TEST_F(MediaCodecVideoDecoderTest,
       SurfaceDestroyedReleasesCodecIfSetSurfaceIsNotSupported) {
  auto* overlay = InitializeWithOverlay();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // MCVD must synchronously release the codec.
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec, NotNull(), _));
  overlay->OnSurfaceDestroyed();
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_F(MediaCodecVideoDecoderTest, PumpCodecPerformsPendingSurfaceTransitions) {
  InitializeWithOverlay();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // Set a pending surface transition and then call Decode() (which calls
  // PumpCodec()).
  surface_chooser_->ProvideSurfaceTexture();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  mcvd_->Decode(nullptr, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest,
       SetSurfaceFailureReleasesTheCodecAndSignalsError) {
  InitializeWithOverlay();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  surface_chooser_->ProvideSurfaceTexture();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(false));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(2);
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec, NotNull(), _));
  mcvd_->Decode(nullptr, decode_cb_.Get());
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_F(MediaCodecVideoDecoderTest, SurfaceTransitionsCanBeCanceled) {
  InitializeWithSurfaceTexture();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // Set a pending transition to an overlay, and then back to a surface texture.
  // They should cancel each other out and leave the codec as-is.
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  auto overlay = base::MakeUnique<MockAndroidOverlay>();
  auto observer = overlay->CreateDestructionObserver();
  surface_chooser_->ProvideOverlay(std::move(overlay));

  // Switching back to surface texture should delete the pending overlay.
  observer->ExpectDestruction();
  surface_chooser_->ProvideSurfaceTexture();
  observer.reset();

  // Verify that Decode() does not transition the surface
  mcvd_->Decode(nullptr, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest, TransitionToSameSurfaceIsIgnored) {
  InitializeWithSurfaceTexture();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  surface_chooser_->ProvideSurfaceTexture();
  mcvd_->Decode(nullptr, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest,
       SurfaceTransitionsAreIgnoredIfSetSurfaceIsNotSupported) {
  InitializeWithSurfaceTexture();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  EXPECT_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  surface_chooser_->ProvideSurfaceTexture();
  mcvd_->Decode(nullptr, decode_cb_.Get());
}

}  // namespace media
