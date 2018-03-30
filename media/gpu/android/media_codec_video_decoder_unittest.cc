// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/media_codec_video_decoder.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/base/android/mock_media_crypto_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/android_video_surface_chooser_impl.h"
#include "media/gpu/android/fake_codec_allocator.h"
#include "media/gpu/android/mock_android_video_surface_chooser.h"
#include "media/gpu/android/mock_device_info.h"
#include "media/gpu/android/mock_surface_texture_gl_owner.h"
#include "media/gpu/android/video_frame_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace media {
namespace {

void OutputCb(scoped_refptr<VideoFrame>* output,
              const scoped_refptr<VideoFrame>& frame) {
  *output = frame;
}

std::unique_ptr<AndroidOverlay> CreateAndroidOverlayCb(
    const base::UnguessableToken&,
    AndroidOverlayConfig) {
  return nullptr;
}

// Make MCVD's destruction observable for teardown tests.
struct DestructionObservableMCVD : public DestructionObservable,
                                   public MediaCodecVideoDecoder {
  using MediaCodecVideoDecoder::MediaCodecVideoDecoder;
};

}  // namespace

class MockVideoFrameFactory : public VideoFrameFactory {
 public:
  MOCK_METHOD2(Initialize, void(bool wants_promotion_hint, InitCb init_cb));
  MOCK_METHOD1(MockSetSurfaceBundle, void(scoped_refptr<AVDASurfaceBundle>));
  MOCK_METHOD6(
      MockCreateVideoFrame,
      void(CodecOutputBuffer* raw_output_buffer,
           scoped_refptr<SurfaceTextureGLOwner> surface_texture,
           base::TimeDelta timestamp,
           gfx::Size natural_size,
           PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
           VideoDecoder::OutputCB output_cb));
  MOCK_METHOD1(MockRunAfterPendingVideoFrames,
               void(base::OnceClosure* closure));
  MOCK_METHOD0(CancelPendingCallbacks, void());

  void SetSurfaceBundle(
      scoped_refptr<AVDASurfaceBundle> surface_bundle) override {
    MockSetSurfaceBundle(surface_bundle);
    if (!surface_bundle) {
      surface_texture_ = nullptr;
    } else {
      surface_texture_ =
          surface_bundle->overlay ? nullptr : surface_bundle->surface_texture;
    }
  }

  void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      VideoDecoder::OutputCB output_cb) override {
    MockCreateVideoFrame(output_buffer.get(), surface_texture_, timestamp,
                         natural_size, promotion_hint_cb, output_cb);
    last_output_buffer_ = std::move(output_buffer);
    output_cb.Run(VideoFrame::CreateBlackFrame(gfx::Size(10, 10)));
  }

  void RunAfterPendingVideoFrames(base::OnceClosure closure) override {
    last_closure_ = std::move(closure);
    MockRunAfterPendingVideoFrames(&last_closure_);
  }

  std::unique_ptr<CodecOutputBuffer> last_output_buffer_;
  scoped_refptr<SurfaceTextureGLOwner> surface_texture_;
  base::OnceClosure last_closure_;
};

class MediaCodecVideoDecoderTest : public testing::Test {
 public:
  MediaCodecVideoDecoderTest() = default;

  void SetUp() override {
    uint8_t data = 0;
    fake_decoder_buffer_ = DecoderBuffer::CopyFrom(&data, 1);
    codec_allocator_ = std::make_unique<FakeCodecAllocator>(
        base::ThreadTaskRunnerHandle::Get());
    device_info_ = std::make_unique<NiceMock<MockDeviceInfo>>();
  }

  void TearDown() override {
    // MCVD calls DeleteSoon() on itself, so we have to run a RunLoop.
    mcvd_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateMcvd() {
    auto surface_chooser =
        std::make_unique<NiceMock<MockAndroidVideoSurfaceChooser>>();
    surface_chooser_ = surface_chooser.get();

    auto surface_texture =
        base::MakeRefCounted<NiceMock<MockSurfaceTextureGLOwner>>(0, nullptr,
                                                                  nullptr);
    surface_texture_ = surface_texture.get();

    auto video_frame_factory =
        std::make_unique<NiceMock<MockVideoFrameFactory>>();
    video_frame_factory_ = video_frame_factory.get();
    // Set up VFF to pass |surface_texture_| via its InitCb.
    const bool want_promotion_hint =
        device_info_->IsSetOutputSurfaceSupported();
    ON_CALL(*video_frame_factory_, Initialize(want_promotion_hint, _))
        .WillByDefault(RunCallback<1>(surface_texture));

    auto* observable_mcvd = new DestructionObservableMCVD(
        gpu_preferences_, device_info_.get(), codec_allocator_.get(),
        std::move(surface_chooser),
        base::BindRepeating(&CreateAndroidOverlayCb),
        base::Bind(&MediaCodecVideoDecoderTest::RequestOverlayInfoCb,
                   base::Unretained(this)),
        std::move(video_frame_factory));
    mcvd_.reset(observable_mcvd);
    mcvd_raw_ = observable_mcvd;
    destruction_observer_ = observable_mcvd->CreateDestructionObserver();
    // Ensure MCVD doesn't leak by default.
    destruction_observer_->ExpectDestruction();
  }

  void CreateCdm(bool require_secure_video_decoder) {
    cdm_ = std::make_unique<MockMediaCryptoContext>();
    require_secure_video_decoder_ = require_secure_video_decoder;

    // We need to send an object as the media crypto, but MCVD shouldn't
    // use it for anything.  Just send in some random java object, so that
    // it's not null.
    media_crypto_ = base::android::ScopedJavaGlobalRef<jobject>(
        gl::SurfaceTexture::Create(0)->j_surface_texture());
  }

  // Just call Initialize(). MCVD will be waiting for a call to Decode() before
  // continuining initialization.
  bool Initialize(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecH264)) {
    if (!mcvd_)
      CreateMcvd();
    bool result = false;
    auto init_cb = [](bool* result_out, bool result) { *result_out = result; };
    mcvd_->Initialize(config, false, cdm_.get(), base::Bind(init_cb, &result),
                      base::BindRepeating(&OutputCb, &most_recent_frame_),
                      VideoDecoder::WaitingForDecryptionKeyCB());
    base::RunLoop().RunUntilIdle();

    if (config.is_encrypted() && cdm_) {
      // If the output is encrypted, then we expect that MCVD will be waiting
      // for the media crypto object.
      // TODO(liberato): why does CreateJavaObjectPtr() not link?
      cdm_->media_crypto_ready_cb.Run(
          std::make_unique<base::android::ScopedJavaGlobalRef<jobject>>(
              media_crypto_),
          require_secure_video_decoder_);
      base::RunLoop().RunUntilIdle();
    }

    return result;
  }

  // Call Initialize() and Decode() to start lazy init. MCVD will be waiting for
  // a codec and have one decode pending.
  MockAndroidOverlay* InitializeWithOverlay_OneDecodePending(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecH264)) {
    Initialize(config);
    mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
    OverlayInfo info;
    info.routing_token = base::UnguessableToken::Deserialize(1, 2);
    provide_overlay_info_cb_.Run(info);
    auto overlay_ptr = std::make_unique<MockAndroidOverlay>();
    auto* overlay = overlay_ptr.get();
    surface_chooser_->ProvideOverlay(std::move(overlay_ptr));
    return overlay;
  }

  // Call Initialize() and Decode() to start lazy init. MCVD will be waiting for
  // a codec and have one decode pending.
  void InitializeWithSurfaceTexture_OneDecodePending(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecH264)) {
    Initialize(config);
    mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
    provide_overlay_info_cb_.Run(OverlayInfo());
    surface_chooser_->ProvideSurfaceTexture();
  }

  // Fully initializes MCVD and returns the codec it's configured with. MCVD
  // will have one decode pending.
  MockMediaCodecBridge* InitializeFully_OneDecodePending(
      VideoDecoderConfig config = TestVideoConfig::Large(kCodecH264)) {
    InitializeWithSurfaceTexture_OneDecodePending(config);
    return codec_allocator_->ProvideMockCodecAsync();
  }

  // Provide access to MCVD's private PumpCodec() to drive the state transitions
  // that depend on queueing and dequeueing buffers. It uses |mcvd_raw_| so that
  // it can be called after |mcvd_| is reset.
  void PumpCodec() { mcvd_raw_->PumpCodec(false); }

  void RequestOverlayInfoCb(
      bool restart_for_transitions,
      const ProvideOverlayInfoCB& provide_overlay_info_cb) {
    restart_for_transitions_ = restart_for_transitions;
    provide_overlay_info_cb_ = provide_overlay_info_cb;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<DecoderBuffer> fake_decoder_buffer_;
  std::unique_ptr<MockDeviceInfo> device_info_;
  std::unique_ptr<FakeCodecAllocator> codec_allocator_;
  MockAndroidVideoSurfaceChooser* surface_chooser_;
  MockSurfaceTextureGLOwner* surface_texture_;
  MockVideoFrameFactory* video_frame_factory_;
  NiceMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb_;
  std::unique_ptr<DestructionObserver> destruction_observer_;
  ProvideOverlayInfoCB provide_overlay_info_cb_;
  bool restart_for_transitions_;
  gpu::GpuPreferences gpu_preferences_;
  scoped_refptr<VideoFrame> most_recent_frame_;

  // This is not an actual media crypto object.
  base::android::ScopedJavaGlobalRef<jobject> media_crypto_;
  bool require_secure_video_decoder_ = false;

  // |mcvd_raw_| lets us call PumpCodec() even after |mcvd_| is dropped, for
  // testing the teardown path.
  MediaCodecVideoDecoder* mcvd_raw_;
  std::unique_ptr<MediaCodecVideoDecoder> mcvd_;
  // This must outlive |mcvd_| .
  std::unique_ptr<MockMediaCryptoContext> cdm_;
};

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
  CreateMcvd();
  EXPECT_CALL(*video_frame_factory_, Initialize(_, _)).Times(0);
  EXPECT_CALL(*surface_chooser_, MockUpdateState()).Times(0);
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, _)).Times(0);
  Initialize();
}

TEST_F(MediaCodecVideoDecoderTest, FirstDecodeTriggersFrameFactoryInit) {
  Initialize();
  EXPECT_CALL(*video_frame_factory_, Initialize(_, _));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest,
       FirstDecodeTriggersOverlayInfoRequestIfSupported) {
  Initialize();
  // Requesting overlay info sets this cb.
  ASSERT_FALSE(provide_overlay_info_cb_);
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_TRUE(provide_overlay_info_cb_);
}

TEST_F(MediaCodecVideoDecoderTest,
       OverlayInfoIsNotRequestedIfOverlaysNotSupported) {
  Initialize();
  ON_CALL(*device_info_, SupportsOverlaySurfaces())
      .WillByDefault(Return(false));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_FALSE(provide_overlay_info_cb_);
}

TEST_F(MediaCodecVideoDecoderTest, RestartForOverlayTransitionsFlagIsCorrect) {
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(true));
  Initialize();
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_FALSE(restart_for_transitions_);
}

TEST_F(MediaCodecVideoDecoderTest,
       OverlayInfoIsNotRequestedIfThreadedTextureMailboxesEnabled) {
  gpu_preferences_.enable_threaded_texture_mailboxes = true;
  Initialize();
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_FALSE(provide_overlay_info_cb_);
}

TEST_F(MediaCodecVideoDecoderTest, OverlayInfoDuringInitUpdatesSurfaceChooser) {
  InitializeWithSurfaceTexture_OneDecodePending();
  EXPECT_CALL(*surface_chooser_, MockUpdateState());
  provide_overlay_info_cb_.Run(OverlayInfo());
}

TEST_F(MediaCodecVideoDecoderTest, CodecIsCreatedAfterSurfaceChosen) {
  Initialize();
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  provide_overlay_info_cb_.Run(OverlayInfo());
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, NotNull()));
  surface_chooser_->ProvideSurfaceTexture();
}

TEST_F(MediaCodecVideoDecoderTest, FrameFactoryInitFailureIsAnError) {
  Initialize();
  ON_CALL(*video_frame_factory_, Initialize(_, _))
      .WillByDefault(RunCallback<1>(nullptr));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(1);
  EXPECT_CALL(*surface_chooser_, MockUpdateState()).Times(0);
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest, CodecCreationFailureIsAnError) {
  InitializeWithSurfaceTexture_OneDecodePending();
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(2);
  // Failing to create a codec should put MCVD into an error state.
  codec_allocator_->ProvideNullCodecAsync();
}

TEST_F(MediaCodecVideoDecoderTest, CodecFailuresAreAnError) {
  auto* codec = InitializeFully_OneDecodePending();
  EXPECT_CALL(*codec, DequeueInputBuffer(_, _))
      .WillOnce(Return(MEDIA_CODEC_ERROR));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR));
  PumpCodec();
}

TEST_F(MediaCodecVideoDecoderTest, AfterInitCompletesTheCodecIsPolled) {
  auto* codec = InitializeFully_OneDecodePending();
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
  auto* codec = InitializeFully_OneDecodePending();
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec, _, _));
}

TEST_F(MediaCodecVideoDecoderTest, SurfaceChooserIsUpdatedOnOverlayChanges) {
  InitializeWithSurfaceTexture_OneDecodePending();

  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory(_)).Times(2);
  OverlayInfo info;
  info.routing_token = base::UnguessableToken::Deserialize(1, 2);
  provide_overlay_info_cb_.Run(info);
  ASSERT_TRUE(surface_chooser_->factory_);
  info.routing_token = base::UnguessableToken::Deserialize(3, 4);
  provide_overlay_info_cb_.Run(info);
  ASSERT_TRUE(surface_chooser_->factory_);
}

TEST_F(MediaCodecVideoDecoderTest, OverlayInfoUpdatesAreIgnoredInStateError) {
  InitializeWithSurfaceTexture_OneDecodePending();
  // Enter the error state.
  codec_allocator_->ProvideNullCodecAsync();

  EXPECT_CALL(*surface_chooser_, MockUpdateState()).Times(0);
  OverlayInfo info;
  info.routing_token = base::UnguessableToken::Deserialize(1, 2);
  provide_overlay_info_cb_.Run(info);
}

TEST_F(MediaCodecVideoDecoderTest, DuplicateOverlayInfoUpdatesAreIgnored) {
  InitializeWithSurfaceTexture_OneDecodePending();

  // The second overlay info update should be ignored.
  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory(_)).Times(1);
  OverlayInfo info;
  info.routing_token = base::UnguessableToken::Deserialize(1, 2);
  provide_overlay_info_cb_.Run(info);
  provide_overlay_info_cb_.Run(info);
}

TEST_F(MediaCodecVideoDecoderTest, CodecIsCreatedWithChosenOverlay) {
  AndroidOverlay* overlay_passed_to_codec = nullptr;
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync(_, _))
      .WillOnce(SaveArg<0>(&overlay_passed_to_codec));
  auto* overlay = InitializeWithOverlay_OneDecodePending();
  DCHECK_EQ(overlay, overlay_passed_to_codec);
}

TEST_F(MediaCodecVideoDecoderTest,
       CodecCreationWeakPtrIsInvalidatedBySurfaceDestroyed) {
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  auto* overlay = InitializeWithOverlay_OneDecodePending();
  overlay->OnSurfaceDestroyed();

  // MCVD should invalidate its CodecAllocatorClient WeakPtr so that it doesn't
  // receive the codec after surface destroyed. FakeCodecAllocator returns
  // nullptr if the client pointer was invalidated.
  ASSERT_FALSE(codec_allocator_->ProvideMockCodecAsync());
}

TEST_F(MediaCodecVideoDecoderTest, SurfaceChangedWhileCodecCreationPending) {
  auto* overlay = InitializeWithOverlay_OneDecodePending();
  overlay->OnSurfaceDestroyed();
  auto codec = std::make_unique<NiceMock<MockMediaCodecBridge>>();

  // SetSurface() is called as soon as the codec is created to switch away from
  // the destroyed surface.
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  codec_allocator_->ProvideMockCodecAsync(std::move(codec));
}

TEST_F(MediaCodecVideoDecoderTest, SurfaceDestroyedDoesSyncSurfaceTransition) {
  auto* overlay = InitializeWithOverlay_OneDecodePending();
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
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  auto* overlay = InitializeWithOverlay_OneDecodePending();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // MCVD must synchronously release the codec.
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec, NotNull(), _));
  overlay->OnSurfaceDestroyed();
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_F(MediaCodecVideoDecoderTest, PumpCodecPerformsPendingSurfaceTransitions) {
  InitializeWithOverlay_OneDecodePending();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // Set a pending surface transition and then call PumpCodec().
  surface_chooser_->ProvideSurfaceTexture();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  PumpCodec();
}

TEST_F(MediaCodecVideoDecoderTest,
       SetSurfaceFailureReleasesTheCodecAndSignalsError) {
  InitializeWithOverlay_OneDecodePending();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  surface_chooser_->ProvideSurfaceTexture();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(false));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(2);
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec, NotNull(), _));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_F(MediaCodecVideoDecoderTest, SurfaceTransitionsCanBeCanceled) {
  InitializeWithSurfaceTexture_OneDecodePending();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // Set a pending transition to an overlay, and then back to a surface texture.
  // They should cancel each other out and leave the codec as-is.
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  auto overlay = std::make_unique<MockAndroidOverlay>();
  auto observer = overlay->CreateDestructionObserver();
  surface_chooser_->ProvideOverlay(std::move(overlay));

  // Switching back to surface texture should delete the pending overlay.
  observer->ExpectDestruction();
  surface_chooser_->ProvideSurfaceTexture();
  observer.reset();

  // Verify that Decode() does not transition the surface
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest, TransitionToSameSurfaceIsIgnored) {
  InitializeWithSurfaceTexture_OneDecodePending();
  auto* codec = codec_allocator_->ProvideMockCodecAsync();
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  surface_chooser_->ProvideSurfaceTexture();
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_F(MediaCodecVideoDecoderTest,
       ResetBeforeCodecInitializedSucceedsImmediately) {
  InitializeWithSurfaceTexture_OneDecodePending();
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
}

TEST_F(MediaCodecVideoDecoderTest, ResetAbortsPendingDecodes) {
  InitializeWithSurfaceTexture_OneDecodePending();
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::ABORTED));
  mcvd_->Reset(base::DoNothing());
}

TEST_F(MediaCodecVideoDecoderTest, ResetAbortsPendingEosDecode) {
  // EOS is treated differently by MCVD. This verifies that it's also aborted.
  auto* codec = InitializeFully_OneDecodePending();
  base::MockCallback<VideoDecoder::DecodeCB> eos_decode_cb;
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), eos_decode_cb.Get());

  // Accept the two pending decodes.
  codec->AcceptOneInput();
  PumpCodec();
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();

  EXPECT_CALL(eos_decode_cb, Run(DecodeStatus::ABORTED));
  mcvd_->Reset(base::DoNothing());
}

TEST_F(MediaCodecVideoDecoderTest, ResetDoesNotFlushAnAlreadyFlushedCodec) {
  auto* codec = InitializeFully_OneDecodePending();

  // The codec is still in the flushed state so Reset() doesn't need to flush.
  EXPECT_CALL(*codec, Flush()).Times(0);
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
}

TEST_F(MediaCodecVideoDecoderTest, ResetDrainsVP8CodecsBeforeFlushing) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(kCodecVP8));
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
  auto* codec = InitializeFully_OneDecodePending();
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

TEST_F(MediaCodecVideoDecoderTest, TeardownCompletesPendingReset) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(kCodecVP8));

  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run()).Times(0);
  mcvd_->Reset(reset_cb.Get());
  EXPECT_CALL(reset_cb, Run());
  mcvd_.reset();

  // VP8 codecs requiring draining for teardown to complete (tested below).
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();
}

TEST_F(MediaCodecVideoDecoderTest, CodecFlushIsDeferredAfterDraining) {
  auto* codec = InitializeFully_OneDecodePending();
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
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  PumpCodec();

  // Releasing the output buffer should now trigger a flush.
  video_frame_factory_->last_output_buffer_.reset();
  EXPECT_CALL(*codec, Flush());
  PumpCodec();
}

TEST_F(MediaCodecVideoDecoderTest, EosDecodeCbIsRunAfterEosIsDequeued) {
  auto* codec = InitializeFully_OneDecodePending();
  codec->AcceptOneInput();
  PumpCodec();

  base::MockCallback<VideoDecoder::DecodeCB> eos_decode_cb;
  EXPECT_CALL(eos_decode_cb, Run(_)).Times(0);
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), eos_decode_cb.Get());
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();

  // On dequeueing EOS, MCVD will post a closure to run eos_decode_cb after
  // pending video frames.
  EXPECT_CALL(*video_frame_factory_, MockRunAfterPendingVideoFrames(_));
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();

  EXPECT_CALL(eos_decode_cb, Run(DecodeStatus::OK));
  std::move(video_frame_factory_->last_closure_).Run();
}

TEST_F(MediaCodecVideoDecoderTest, TeardownBeforeInitWorks) {
  // Since we assert that MCVD is destructed by default, this test verifies that
  // MCVD is destructed safely before Initialize().
}

TEST_F(MediaCodecVideoDecoderTest, TeardownInvalidatesCodecCreationWeakPtr) {
  InitializeWithSurfaceTexture_OneDecodePending();
  destruction_observer_->DoNotAllowDestruction();
  mcvd_.reset();
  // DeleteSoon() is now pending. Ensure it's safe if the codec creation
  // completes before it runs.
  ASSERT_FALSE(codec_allocator_->ProvideMockCodecAsync());
  destruction_observer_->ExpectDestruction();
}

TEST_F(MediaCodecVideoDecoderTest, TeardownDoesNotDrainFlushedCodecs) {
  InitializeFully_OneDecodePending();
  // Since we assert that MCVD is destructed by default, this test verifies that
  // MCVD is destructed without requiring the codec to output an EOS buffer.
}

TEST_F(MediaCodecVideoDecoderTest, TeardownDoesNotDrainNonVp8Codecs) {
  auto* codec = InitializeFully_OneDecodePending();
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();
  // Since we assert that MCVD is destructed by default, this test verifies that
  // MCVD is destructed without requiring the codec to output an EOS buffer.
}

TEST_F(MediaCodecVideoDecoderTest, TeardownDrainsVp8CodecsBeforeDestruction) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(kCodecVP8));
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // MCVD should not be destructed immediately.
  destruction_observer_->DoNotAllowDestruction();
  mcvd_.reset();
  base::RunLoop().RunUntilIdle();

  // It should be destructed after draining completes.
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  EXPECT_CALL(*codec, Flush()).Times(0);
  destruction_observer_->ExpectDestruction();
  PumpCodec();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaCodecVideoDecoderTest, CdmInitializationWorksForL3) {
  // Make sure that MCVD uses the cdm, and sends it along to the codec.
  CreateCdm(false);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  InitializeWithOverlay_OneDecodePending(
      TestVideoConfig::NormalEncrypted(kCodecH264));
  ASSERT_TRUE(!!cdm_->new_key_cb);
  ASSERT_TRUE(!!cdm_->cdm_unset_cb);
  ASSERT_TRUE(!!cdm_->media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, true);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, false);
  ASSERT_FALSE(codec_allocator_->most_recent_config->requires_secure_codec);
  // We can't check for equality safely, but verify that something was provided.
  ASSERT_TRUE(codec_allocator_->most_recent_config->media_crypto->obj());

  // When |mcvd_| is destroyed, expect that it will unregister itself.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
}

TEST_F(MediaCodecVideoDecoderTest, CdmInitializationWorksForL1) {
  // Make sure that MCVD uses the cdm, and sends it along to the codec.
  CreateCdm(true);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  InitializeWithOverlay_OneDecodePending(
      TestVideoConfig::NormalEncrypted(kCodecH264));
  ASSERT_TRUE(!!cdm_->new_key_cb);
  ASSERT_TRUE(!!cdm_->cdm_unset_cb);
  ASSERT_TRUE(!!cdm_->media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, true);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, true);
  ASSERT_TRUE(codec_allocator_->most_recent_config->requires_secure_codec);
  ASSERT_TRUE(codec_allocator_->most_recent_config->media_crypto->obj());

  // When |mcvd_| is destroyed, expect that it will unregister itself.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
}

TEST_F(MediaCodecVideoDecoderTest, CdmIsIgnoredIfNotEncrypted) {
  CreateCdm(true);
  // It should not register or unregister.
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _)).Times(0);
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId))
      .Times(0);
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));
  ASSERT_TRUE(!cdm_->new_key_cb);
  ASSERT_TRUE(!cdm_->cdm_unset_cb);
  ASSERT_TRUE(!cdm_->media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, false);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, false);
}

TEST_F(MediaCodecVideoDecoderTest, MissingMediaCryptoFailsInit) {
  // Encrypted media that doesn't get a mediacrypto should fail to init.
  CreateCdm(true);
  media_crypto_ = nullptr;
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  ASSERT_FALSE(Initialize(TestVideoConfig::NormalEncrypted(kCodecH264)));
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
}

TEST_F(MediaCodecVideoDecoderTest, MissingCdmFailsInit) {
  // MCVD should fail init if we don't provide a cdm with an encrypted config.
  ASSERT_FALSE(Initialize(TestVideoConfig::NormalEncrypted(kCodecH264)));
}

TEST_F(MediaCodecVideoDecoderTest, VideoFramesArePowerEfficient) {
  // MCVD should mark video frames as POWER_EFFICIENT.
  auto* codec = InitializeFully_OneDecodePending();

  // Produce one output.
  codec->AcceptOneInput();
  codec->ProduceOneOutput();
  EXPECT_CALL(*video_frame_factory_, MockCreateVideoFrame(_, _, _, _, _, _));
  PumpCodec();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!most_recent_frame_);
  bool power_efficient = false;
  EXPECT_TRUE(most_recent_frame_->metadata()->GetBoolean(
      VideoFrameMetadata::POWER_EFFICIENT, &power_efficient));
  EXPECT_TRUE(power_efficient);
}

}  // namespace media
