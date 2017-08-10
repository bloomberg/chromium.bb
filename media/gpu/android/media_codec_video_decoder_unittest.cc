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
#include "media/base/gmock_callback_support.h"
#include "media/base/test_helpers.h"
#include "media/gpu/android/fake_codec_allocator.h"
#include "media/gpu/android/mock_device_info.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/android_video_surface_chooser_impl.h"
#include "media/gpu/fake_android_video_surface_chooser.h"
#include "media/gpu/mock_surface_texture_gl_owner.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace media {
namespace {

void OutputCb(const scoped_refptr<VideoFrame>& frame) {}

void OutputWithReleaseMailboxCb(VideoFrameFactory::ReleaseMailboxCB release_cb,
                                const scoped_refptr<VideoFrame>& frame) {}

gpu::GpuCommandBufferStub* GetStubCb() {
  return nullptr;
}

}  // namespace

class MockVideoFrameFactory : public VideoFrameFactory {
 public:
  MOCK_METHOD3(Initialize,
               void(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                    GetStubCb get_stub_cb,
                    InitCb init_cb));
  MOCK_METHOD5(MockCreateVideoFrame,
               void(CodecOutputBuffer* raw_output_buffer,
                    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
                    base::TimeDelta timestamp,
                    gfx::Size natural_size,
                    OutputWithReleaseMailboxCB output_cb));

  void CreateVideoFrame(std::unique_ptr<CodecOutputBuffer> output_buffer,
                        scoped_refptr<SurfaceTextureGLOwner> surface_texture,
                        base::TimeDelta timestamp,
                        gfx::Size natural_size,
                        OutputWithReleaseMailboxCB output_cb) override {
    MockCreateVideoFrame(output_buffer.get(), surface_texture, timestamp,
                         natural_size, output_cb);
    last_output_buffer_ = std::move(output_buffer);
  }

  std::unique_ptr<CodecOutputBuffer> last_output_buffer_;
};

class MediaCodecVideoDecoderTest : public testing::Test {
 public:
  MediaCodecVideoDecoderTest() = default;

  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    RegisterJni(env);

    uint8_t data = 0;
    fake_decoder_buffer_ = DecoderBuffer::CopyFrom(&data, 1);
    codec_allocator_ = base::MakeUnique<FakeCodecAllocator>();
    device_info_ = base::MakeUnique<NiceMock<MockDeviceInfo>>();
    auto surface_chooser = base::MakeUnique<NiceMock<FakeSurfaceChooser>>();
    surface_chooser_ = surface_chooser.get();

    auto surface_texture = make_scoped_refptr(
        new NiceMock<MockSurfaceTextureGLOwner>(0, nullptr, nullptr));
    surface_texture_ = surface_texture.get();

    auto video_frame_factory =
        base::MakeUnique<NiceMock<MockVideoFrameFactory>>();
    video_frame_factory_ = video_frame_factory.get();
    // Set up VFF to pass |surface_texture_| via its InitCb.
    ON_CALL(*video_frame_factory_, Initialize(_, _, _))
        .WillByDefault(RunCallback<2>(surface_texture));

    mcvd_ = base::MakeUnique<MediaCodecVideoDecoder>(
        base::ThreadTaskRunnerHandle::Get(), base::Bind(&GetStubCb),
        base::Bind(&OutputWithReleaseMailboxCb), device_info_.get(),
        codec_allocator_.get(), std::move(surface_chooser),
        std::move(video_frame_factory));
  }

  // Just call Initialize(). MCVD will be waiting for a call to Decode() before
  // continuining initialization.
  bool Initialize(const VideoDecoderConfig& config) {
    bool result = false;
    auto init_cb = [](bool* result_out, bool result) { *result_out = result; };
    mcvd_->Initialize(config, false, nullptr, base::Bind(init_cb, &result),
                      base::Bind(&OutputCb));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  // Call Initialize() and Decode() to start lazy init. MCVD will be waiting for
  // a codec and have one decode pending.
  MockAndroidOverlay* InitializeWithOverlay(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecVP8)) {
    Initialize(config);
    mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
    auto overlay_ptr = base::MakeUnique<MockAndroidOverlay>();
    auto* overlay = overlay_ptr.get();
    surface_chooser_->ProvideOverlay(std::move(overlay_ptr));
    return overlay;
  }

  // Call Initialize() and Decode() to start lazy init. MCVD will be waiting for
  // a codec and have one decode pending.
  void InitializeWithSurfaceTexture(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecVP8)) {
    Initialize(config);
    mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
    surface_chooser_->ProvideSurfaceTexture();
  }

  // Fully initializes MCVD and returns the codec it's configured with. MCVD
  // will have one decode pending.
  MockMediaCodecBridge* InitializeFully(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecVP8)) {
    InitializeWithSurfaceTexture(config);
    return codec_allocator_->ProvideMockCodecAsync();
  }

  // Provide access to MCVD's private PumpCodec() to avoid trying to make its
  // repeating task run in the test environment.
  void PumpCodec() { mcvd_->PumpCodec(false); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<DecoderBuffer> fake_decoder_buffer_;
  std::unique_ptr<MockDeviceInfo> device_info_;
  std::unique_ptr<FakeCodecAllocator> codec_allocator_;
  FakeSurfaceChooser* surface_chooser_;
  MockSurfaceTextureGLOwner* surface_texture_;
  MockVideoFrameFactory* video_frame_factory_;
  NiceMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb_;

  // mcvd_ is last so it's destructed first.
  std::unique_ptr<MediaCodecVideoDecoder> mcvd_;
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
  EXPECT_CALL(*video_frame_factory_, Initialize(_, _, _)).Times(0);
  EXPECT_CALL(*surface_chooser_, MockInitialize()).Times(0);
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, _)).Times(0);
  Initialize(TestVideoConfig::NormalH264());
}

TEST_F(MediaCodecVideoDecoderTest, FirstDecodeTriggersFrameFactoryInit) {
  Initialize(TestVideoConfig::NormalH264());
  EXPECT_CALL(*video_frame_factory_, Initialize(_, _, _));
  mcvd_->Decode(nullptr, decode_cb_.Get());
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

TEST_F(MediaCodecVideoDecoderTest, FrameFactoryInitFailureIsAnError) {
  Initialize(TestVideoConfig::NormalH264());
  ON_CALL(*video_frame_factory_, Initialize(_, _, _))
      .WillByDefault(RunCallback<2>(nullptr));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(1);
  EXPECT_CALL(*surface_chooser_, MockInitialize()).Times(0);
  mcvd_->Decode(nullptr, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest, CodecCreationFailureIsAnError) {
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

  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory(_)).Times(2);
  OverlayInfo info;
  info.surface_id = 123;
  mcvd_->SetOverlayInfo(info);
  info.surface_id = 456;
  mcvd_->SetOverlayInfo(info);
}

TEST_F(MediaCodecVideoDecoderTest, DuplicateSetOverlayInfosAreIgnored) {
  InitializeWithOverlay();

  // The second SetOverlayInfo() should be ignored.
  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory(_)).Times(1);
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

  // Set a pending surface transition and then call PumpCodec().
  surface_chooser_->ProvideSurfaceTexture();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  PumpCodec();
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

TEST_F(MediaCodecVideoDecoderTest,
       ResetBeforeCodecInitializedSucceedsImmediately) {
  InitializeWithSurfaceTexture();
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
}

TEST_F(MediaCodecVideoDecoderTest, ResetAbortsPendingDecodes) {
  InitializeWithSurfaceTexture();
  // One decode is now pending.
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::ABORTED));
  mcvd_->Reset(base::Bind(&base::DoNothing));
}

TEST_F(MediaCodecVideoDecoderTest, ResetAbortsPendingEosDecode) {
  // EOS is treated differently by MCVD. This verifies that it's also aborted.
  auto* codec = InitializeFully();
  base::MockCallback<VideoDecoder::DecodeCB> eos_decode_cb;
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), eos_decode_cb.Get());

  // Accept the two pending decodes.
  codec->AcceptOneInput();
  PumpCodec();
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();

  EXPECT_CALL(eos_decode_cb, Run(DecodeStatus::ABORTED));
  mcvd_->Reset(base::Bind(&base::DoNothing));
}

TEST_F(MediaCodecVideoDecoderTest, ResetDoesNotFlushAnAlreadyFlushedCodec) {
  auto* codec = InitializeFully();

  // The codec is still in the flushed state so Reset() doesn't need to flush.
  EXPECT_CALL(*codec, Flush()).Times(0);
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
}

TEST_F(MediaCodecVideoDecoderTest, ResetDrainsVP8CodecsBeforeFlushing) {
  auto* codec = InitializeFully();
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // The reset should not complete immediately because the codec needs to be
  // drained.
  EXPECT_CALL(*codec, Flush()).Times(0);
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run()).Times(0);
  mcvd_->Reset(reset_cb.Get());

  // The next input should be an EOS.
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();
  testing::Mock::VerifyAndClearExpectations(codec);

  // After the EOS is dequeued, the reset should complete.
  EXPECT_CALL(reset_cb, Run());
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();
  testing::Mock::VerifyAndClearExpectations(&reset_cb);
}

TEST_F(MediaCodecVideoDecoderTest, ResetDoesNotDrainNonVp8Codecs) {
  auto* codec = InitializeFully(TestVideoConfig::NormalH264());
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // The reset should complete immediately because the codec is not VP8 so
  // it doesn't need draining.
  EXPECT_CALL(*codec, Flush());
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
}

TEST_F(MediaCodecVideoDecoderTest, DestructionCompletesPendingReset) {
  auto* codec = InitializeFully();
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run()).Times(0);
  mcvd_->Reset(reset_cb.Get());
  EXPECT_CALL(reset_cb, Run());
  mcvd_.reset();
}

TEST_F(MediaCodecVideoDecoderTest, CodecFlushIsDeferredAfterDraining) {
  auto* codec = InitializeFully();
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), decode_cb_.Get());

  // Produce one output that VFF will hold onto.
  codec->AcceptOneInput();
  codec->ProduceOneOutput();
  PumpCodec();

  // Drain the codec.
  EXPECT_CALL(*codec, Flush()).Times(0);
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();

  // Create a pending decode. The codec should still not be flushed because
  // there is an unrendered output buffer.
  mcvd_->Decode(nullptr, decode_cb_.Get());
  PumpCodec();

  // Releasing the output buffer should now trigger a flush.
  video_frame_factory_->last_output_buffer_.reset();
  EXPECT_CALL(*codec, Flush());
  PumpCodec();
}

TEST_F(MediaCodecVideoDecoderTest, EosDecodeCbIsRunAfterEosIsDequeued) {
  auto* codec = InitializeFully();
  base::MockCallback<VideoDecoder::DecodeCB> eos_decode_cb;
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), eos_decode_cb.Get());

  codec->AcceptOneInput();
  PumpCodec();
  EXPECT_CALL(eos_decode_cb, Run(_)).Times(0);
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();

  EXPECT_CALL(eos_decode_cb, Run(_));
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();
  // eos_codec_cb is posted to the gpu task runner which is this thread's task
  // runner in the test.
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
