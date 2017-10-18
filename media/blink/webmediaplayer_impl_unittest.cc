// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/test/test_context_provider.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "media/renderers/default_renderer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebSurfaceLayerBridge.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

// Specify different values for testing.
const base::TimeDelta kMaxKeyframeDistanceToDisableBackgroundVideo =
    base::TimeDelta::FromSeconds(5);
const base::TimeDelta kMaxKeyframeDistanceToDisableBackgroundVideoMSE =
    base::TimeDelta::FromSeconds(10);

int64_t OnAdjustAllocatedMemory(int64_t delta) {
  return 0;
}

MATCHER(WmpiDestroyed, "") {
  return CONTAINS_STRING(arg, "WEBMEDIAPLAYER_DESTROYED {}");
}

MATCHER_P2(PlaybackRateChanged, old_rate_string, new_rate_string, "") {
  return CONTAINS_STRING(arg, "Effective playback rate changed from " +
                                  std::string(old_rate_string) + " to " +
                                  std::string(new_rate_string));
}

mojom::VideoDecodeStatsRecorderPtr CreateCapabilitiesRecorder() {
  mojom::VideoDecodeStatsRecorderPtr recorder;
  VideoDecodeStatsRecorder::Create(mojo::MakeRequest(&recorder));
  return recorder;
}

class DummyWebMediaPlayerClient : public blink::WebMediaPlayerClient {
 public:
  DummyWebMediaPlayerClient() {}

  // blink::WebMediaPlayerClient implementation.
  void NetworkStateChanged() override {}
  void ReadyStateChanged() override {}
  void TimeChanged() override {}
  void Repaint() override {}
  void DurationChanged() override {}
  void SizeChanged() override {}
  void PlaybackStateChanged() override {}
  void SetWebLayer(blink::WebLayer*) override {}
  blink::WebMediaPlayer::TrackId AddAudioTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::AudioTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool enabled) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void RemoveAudioTrack(blink::WebMediaPlayer::TrackId) override {}
  blink::WebMediaPlayer::TrackId AddVideoTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::VideoTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool selected) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void RemoveVideoTrack(blink::WebMediaPlayer::TrackId) override {}
  void AddTextTrack(blink::WebInbandTextTrack*) override {}
  void RemoveTextTrack(blink::WebInbandTextTrack*) override {}
  void MediaSourceOpened(blink::WebMediaSource*) override {}
  void RequestSeek(double) override {}
  void RemoteRouteAvailabilityChanged(
      blink::WebRemotePlaybackAvailability) override {}
  void ConnectedToRemoteDevice() override {}
  void DisconnectedFromRemoteDevice() override {}
  void CancelledRemotePlaybackRequest() override {}
  void RemotePlaybackStarted() override {}
  void RemotePlaybackCompatibilityChanged(const blink::WebURL& url,
                                          bool is_compatible) override {}
  void OnBecamePersistentVideo(bool) override {}
  bool IsAutoplayingMuted() override { return is_autoplaying_muted_; }
  bool HasSelectedVideoTrack() override { return false; }
  blink::WebMediaPlayer::TrackId GetSelectedVideoTrackId() override {
    return blink::WebMediaPlayer::TrackId();
  }
  bool HasNativeControls() override { return false; }
  bool IsAudioElement() override { return false; }
  blink::WebMediaPlayer::DisplayType DisplayType() const override {
    return blink::WebMediaPlayer::DisplayType::kInline;
  }
  void ActivateViewportIntersectionMonitoring(bool activate) override {}
  void MediaRemotingStarted(
      const blink::WebString& remote_device_friendly_name) override {}
  void MediaRemotingStopped() override {}

  void set_is_autoplaying_muted(bool value) { is_autoplaying_muted_ = value; }

 private:
  bool is_autoplaying_muted_ = false;

  DISALLOW_COPY_AND_ASSIGN(DummyWebMediaPlayerClient);
};

class MockWebMediaPlayerDelegate : public WebMediaPlayerDelegate {
 public:
  MockWebMediaPlayerDelegate() = default;
  ~MockWebMediaPlayerDelegate() = default;

  // WebMediaPlayerDelegate implementation.
  int AddObserver(Observer* observer) override {
    DCHECK_EQ(nullptr, observer_);
    observer_ = observer;
    return player_id_;
  }

  void RemoveObserver(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    observer_ = nullptr;
  }

  MOCK_METHOD4(DidPlay, void(int, bool, bool, MediaContentType));
  MOCK_METHOD1(DidPause, void(int));
  MOCK_METHOD1(PlayerGone, void(int));

  void SetIdle(int player_id, bool is_idle) override {
    DCHECK_EQ(player_id_, player_id);
    is_idle_ = is_idle;
    is_stale_ &= is_idle;
  }

  bool IsIdle(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_idle_;
  }

  void DidPlayerMutedStatusChange(int delegate_id, bool muted) override {
    DCHECK_EQ(player_id_, delegate_id);
  }

  void ClearStaleFlag(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    is_stale_ = false;
  }

  bool IsStale(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_stale_;
  }

  void SetIsEffectivelyFullscreen(int player_id, bool value) override {
    DCHECK_EQ(player_id_, player_id);
  }

  void DidPlayerSizeChange(int player_id, const gfx::Size& size) override {
    DCHECK_EQ(player_id_, player_id);
  }

  bool IsFrameHidden() override { return is_hidden_; }

  bool IsFrameClosed() override { return is_closed_; }

  void SetIdleForTesting(bool is_idle) { is_idle_ = is_idle; }

  void SetStaleForTesting(bool is_stale) {
    is_idle_ |= is_stale;
    is_stale_ = is_stale;
  }

  // Returns true if the player does in fact expire.
  bool ExpireForTesting() {
    if (is_idle_ && !is_stale_) {
      is_stale_ = true;
      observer_->OnIdleTimeout();
    }

    return is_stale_;
  }

  void SetFrameHiddenForTesting(bool is_hidden) { is_hidden_ = is_hidden; }

  void SetFrameClosedForTesting(bool is_closed) { is_closed_ = is_closed; }

 private:
  Observer* observer_ = nullptr;
  int player_id_ = 1234;
  bool is_idle_ = false;
  bool is_stale_ = false;
  bool is_hidden_ = false;
  bool is_closed_ = false;
};

class MockSurfaceLayerBridge : public blink::WebSurfaceLayerBridge {
 public:
  MOCK_CONST_METHOD0(GetWebLayer, blink::WebLayer*());
  MOCK_CONST_METHOD0(GetFrameSinkId, const viz::FrameSinkId&());
};

class WebMediaPlayerImplTest : public testing::Test {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        web_view_(
            blink::WebView::Create(nullptr,
                                   blink::kWebPageVisibilityStateVisible)),
        web_local_frame_(
            blink::WebLocalFrame::CreateMainFrame(web_view_,
                                                  &web_frame_client_,
                                                  nullptr,
                                                  nullptr)),
        context_provider_(cc::TestContextProvider::Create()),
        audio_parameters_(TestAudioParameters::Normal()) {
    media_thread_.StartAndWaitForTesting();
  }

  void InitializeWebMediaPlayerImpl() {
    auto media_log = std::make_unique<NiceMock<MockMediaLog>>();

    // Retain a raw pointer to |media_log| for use by tests. Meanwhile, give its
    // ownership to |wmpi_|. Reject attempts to reinitialize to prevent orphaned
    // expectations on previous |media_log_|.
    ASSERT_FALSE(media_log_) << "Reinitialization of media_log_ is disallowed";
    media_log_ = media_log.get();

    auto factory_selector = base::MakeUnique<RendererFactorySelector>();
    factory_selector->AddFactory(
        RendererFactorySelector::FactoryType::DEFAULT,
        base::MakeUnique<DefaultRendererFactory>(
            media_log.get(), nullptr,
            DefaultRendererFactory::GetGpuFactoriesCB()));
    factory_selector->SetBaseFactoryType(
        RendererFactorySelector::FactoryType::DEFAULT);

    WatchTimeRecorder::CreateWatchTimeRecorderProvider(
        mojo::MakeRequest(&provider_));

    wmpi_ = base::MakeUnique<WebMediaPlayerImpl>(
        web_local_frame_, &client_, nullptr, &delegate_,
        std::move(factory_selector), url_index_.get(),
        base::MakeUnique<WebMediaPlayerParams>(
            std::move(media_log), WebMediaPlayerParams::DeferLoadCB(),
            scoped_refptr<SwitchableAudioRendererSink>(),
            media_thread_.task_runner(), message_loop_.task_runner(),
            message_loop_.task_runner(), base::Bind(&OnAdjustAllocatedMemory),
            nullptr, nullptr, RequestRoutingTokenCallback(), nullptr,
            kMaxKeyframeDistanceToDisableBackgroundVideo,
            kMaxKeyframeDistanceToDisableBackgroundVideoMSE, false, false,
            provider_.get(), base::Bind(&CreateCapabilitiesRecorder),
            base::Bind(&WebMediaPlayerImplTest::CreateMockSurfaceLayerBridge,
                       base::Unretained(this)),
            base::BindRepeating(&WebMediaPlayerImplTest::ProvideContext,
                                base::Unretained(this)),
            cc::TestContextProvider::Create()));
}

  ~WebMediaPlayerImplTest() override {
    // Destruct WebMediaPlayerImpl and pump the message loop to ensure that
    // objects passed to the message loop for destruction are released.
    //
    // NOTE: This should be done before any other member variables are
    // destructed since WMPI may reference them during destruction.
    wmpi_.reset();

    base::RunLoop().RunUntilIdle();

    web_view_->Close();
  }

  void ProvideContext(
      base::OnceCallback<void(viz::ContextProvider*)> callback) {
    media_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::Unretained(context_provider_.get())));
  }

 protected:
  std::unique_ptr<blink::WebSurfaceLayerBridge> CreateMockSurfaceLayerBridge(
      blink::WebSurfaceLayerBridgeObserver*) {
    return base::WrapUnique<blink::WebSurfaceLayerBridge>(
        surface_layer_bridge_);
  }

  void SetNetworkState(blink::WebMediaPlayer::NetworkState state) {
    wmpi_->SetNetworkState(state);
  }

  void SetReadyState(blink::WebMediaPlayer::ReadyState state) {
    wmpi_->SetReadyState(state);
  }

  void SetDuration(base::TimeDelta value) {
    wmpi_->SetPipelineMediaDurationForTest(value);
  }

  base::TimeDelta GetCurrentTimeInternal() {
    return wmpi_->GetCurrentTimeInternal();
  }

  void SetPaused(bool is_paused) { wmpi_->paused_ = is_paused; }
  void SetSeeking(bool is_seeking) { wmpi_->seeking_ = is_seeking; }
  void SetEnded(bool is_ended) { wmpi_->ended_ = is_ended; }
  void SetTickClock(base::TickClock* clock) {
    wmpi_->SetTickClockForTest(clock);
  }

  void SetFullscreen(bool is_fullscreen) {
    wmpi_->overlay_enabled_ = is_fullscreen;
  }

  void SetMetadata(bool has_audio, bool has_video) {
    wmpi_->SetNetworkState(blink::WebMediaPlayer::kNetworkStateLoaded);
    wmpi_->SetReadyState(blink::WebMediaPlayer::kReadyStateHaveMetadata);
    wmpi_->pipeline_metadata_.has_audio = has_audio;
    wmpi_->pipeline_metadata_.has_video = has_video;

    if (has_video)
      wmpi_->pipeline_metadata_.video_decoder_config =
          TestVideoConfig::Normal();

    if (has_audio)
      wmpi_->pipeline_metadata_.audio_decoder_config =
          TestAudioConfig::Normal();
  }

  void SetError(PipelineStatus status = PIPELINE_ERROR_DECODE) {
    wmpi_->OnError(status);
  }

  void OnMetadata(PipelineMetadata metadata) { wmpi_->OnMetadata(metadata); }

  void OnVideoNaturalSizeChange(const gfx::Size& size) {
    wmpi_->OnVideoNaturalSizeChange(size);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_FrameHidden() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Suspended() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, true, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Remote() {
    return wmpi_->UpdatePlayState_ComputePlayState(true, true, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_BackgroundedStreaming() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, true);
  }

  bool IsSuspended() { return wmpi_->pipeline_controller_.IsSuspended(); }

  void AddBufferedRanges() {
    wmpi_->buffered_data_source_host_.AddBufferedByteRange(0, 1);
  }

  void SetDelegateState(WebMediaPlayerImpl::DelegateState state) {
    wmpi_->SetDelegateState(state, false);
  }

  void SetUpMediaSuspend(bool enable) {
#if defined(OS_ANDROID)
    if (!enable) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kDisableMediaSuspend);
    }
#else
    if (enable) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableMediaSuspend);
    }
#endif
  }

  bool IsVideoLockedWhenPausedWhenHidden() const {
    return wmpi_->video_locked_when_paused_when_hidden_;
  }

  void BackgroundPlayer() {
    delegate_.SetFrameHiddenForTesting(true);
    delegate_.SetFrameClosedForTesting(false);
    wmpi_->OnFrameHidden();
  }

  void ForegroundPlayer() {
    delegate_.SetFrameHiddenForTesting(false);
    delegate_.SetFrameClosedForTesting(false);
    wmpi_->OnFrameShown();
  }

  void Play() { wmpi_->Play(); }

  void Pause() { wmpi_->Pause(); }

  void ScheduleIdlePauseTimer() { wmpi_->ScheduleIdlePauseTimer(); }

  bool IsIdlePauseTimerRunning() {
    return wmpi_->background_pause_timer_.IsRunning();
  }

  void SetSuspendState(bool is_suspended) {
    wmpi_->SetSuspendState(is_suspended);
  }

  void SetLoadType(blink::WebMediaPlayer::LoadType load_type) {
    wmpi_->load_type_ = load_type;
  }

  bool IsVideoTrackDisabled() const { return wmpi_->video_track_disabled_; }

  bool IsDisableVideoTrackPending() const {
    return !wmpi_->update_background_status_cb_.IsCancelled();
  }

  // "Renderer" thread.
  base::MessageLoop message_loop_;

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  blink::WebFrameClient web_frame_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;

  scoped_refptr<cc::TestContextProvider> context_provider_;

  std::unique_ptr<media::UrlIndex> url_index_;

  // Audio hardware configuration.
  AudioParameters audio_parameters_;

  // The client interface used by |wmpi_|. Just a dummy for now, but later we
  // may want a mock or intelligent fake.
  DummyWebMediaPlayerClient client_;

  NiceMock<MockWebMediaPlayerDelegate> delegate_;

  mojom::WatchTimeRecorderProviderPtr provider_;

  StrictMock<MockSurfaceLayerBridge>* surface_layer_bridge_ = nullptr;

  // Only valid once set by InitializeWebMediaPlayerImpl(), this is for
  // verifying a subset of potential media logs.
  NiceMock<MockMediaLog>* media_log_ = nullptr;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendBeforeLoadingBegins) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(delegate_.ExpireForTesting());
}

TEST_F(WebMediaPlayerImplTest,
       IdleSuspendIsDisabledIfLoadingProgressedRecently) {
  InitializeWebMediaPlayerImpl();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(clock);
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  // Advance less than the loading timeout.
  clock->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledIfLoadingHasStalled) {
  InitializeWebMediaPlayerImpl();
  SetNetworkState(blink::WebMediaPlayer::kNetworkStateLoading);
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(clock);
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  // Advance more than the loading timeout.
  clock->Advance(base::TimeDelta::FromSeconds(4));
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, DidLoadingProgressTriggersResume) {
  // Same setup as IdleSuspendIsEnabledBeforeLoadingBegins.
  InitializeWebMediaPlayerImpl();
  SetNetworkState(blink::WebMediaPlayer::kNetworkStateLoading);
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());

  // Like IdleSuspendIsDisabledIfLoadingProgressedRecently, the idle timeout
  // should be rejected if it hasn't been long enough.
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  EXPECT_FALSE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Constructed) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_HaveMetadata) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_HaveFutureData) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

// Ensure memory reporting is not running after an error.
TEST_F(WebMediaPlayerImplTest, ComputePlayState_PlayingError) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  SetError();
  state = ComputePlayState();
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PlayingVideoOnly) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(false, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Underflow) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveCurrentData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHidden) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenAudioOnly) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  SetMetadata(true, false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenSuspendNoResume) {
  SetUpMediaSuspend(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);

  SetPaused(true);
  state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenSuspendWithResume) {
  SetUpMediaSuspend(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameClosed) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  delegate_.SetFrameClosedForTesting(true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PausedSeek) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetSeeking(true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Ended) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  SetEnded(true);

  // Before Blink pauses us (or seeks for looping content), the media session
  // should be preserved.
  WebMediaPlayerImpl::PlayState state;
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);

  SetPaused(true);
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_StaysSuspended) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);

  // Should stay suspended even though not stale or backgrounded.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Suspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Remote) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);

  // Remote media is always suspended.
  // TODO(sandersd): Decide whether this should count as idle or not.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Remote();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Fullscreen) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetFullscreen(true);
  SetPaused(true);
  delegate_.SetStaleForTesting(true);

  // Fullscreen media is never suspended (Android only behavior).
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Streaming) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(true);
  delegate_.SetStaleForTesting(true);

  // Streaming media should not suspend, even if paused, stale, and
  // backgrounded.
  WebMediaPlayerImpl::PlayState state;
  state = ComputePlayState_BackgroundedStreaming();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);

  // Streaming media should suspend when the tab is closed, regardless.
  delegate_.SetFrameClosedForTesting(true);
  state = ComputePlayState_BackgroundedStreaming();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_StartsAndStops) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  client_.set_is_autoplaying_muted(true);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, _));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  client_.set_is_autoplaying_muted(false);
  EXPECT_CALL(delegate_, DidPlay(_, true, true, _));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_SetVolume) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  client_.set_is_autoplaying_muted(true);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, _));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  client_.set_is_autoplaying_muted(false);
  EXPECT_CALL(delegate_, DidPlay(_, true, true, _));
  wmpi_->SetVolume(1.0);
}

TEST_F(WebMediaPlayerImplTest, NoStreams) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;

  // Nothing should happen.  In particular, no assertions should fail.
  OnMetadata(metadata);
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  metadata.natural_size = gfx::Size(320, 240);

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->NaturalSize());

  // TODO(sandersd): Verify that the client is notified of the size change?
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1920, 1080), wmpi_->NaturalSize());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange_Rotated) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalRotated(VIDEO_ROTATION_90);
  metadata.natural_size = gfx::Size(320, 240);

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->NaturalSize());

  // For 90/270deg rotations, the natural size should be transposed.
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1080, 1920), wmpi_->NaturalSize());
}

TEST_F(WebMediaPlayerImplTest, VideoLockedWhenPausedWhenHidden) {
  InitializeWebMediaPlayerImpl();

  // Setting metadata initializes |watch_time_reporter_| used in play().
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  OnMetadata(metadata);

  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // Backgrounding the player sets the lock.
  BackgroundPlayer();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // Play without a user gesture doesn't unlock the player.
  Play();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture it does unlock the player.
  {
    blink::WebScopedUserGesture user_gesture(nullptr);
    Play();
    EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());
  }

  // Pause without a user gesture doesn't lock the player.
  Pause();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture, pause does lock the player.
  {
    blink::WebScopedUserGesture user_gesture(nullptr);
    Pause();
    EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());
  }

  // Foregrounding the player unsets the lock.
  ForegroundPlayer();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());
}

TEST_F(WebMediaPlayerImplTest, BackgroundIdlePauseTimerDependsOnAudio) {
  InitializeWebMediaPlayerImpl();
  SetSuspendState(true);
  SetPaused(false);

  ASSERT_TRUE(IsSuspended());

  // Video-only players are not paused when suspended.
  SetMetadata(false, true);
  ScheduleIdlePauseTimer();
  EXPECT_FALSE(IsIdlePauseTimerRunning());

  SetMetadata(true, true);
  ScheduleIdlePauseTimer();
  EXPECT_TRUE(IsIdlePauseTimerRunning());
}

// Verifies that an infinite duration doesn't muck up GetCurrentTimeInternal.
TEST_F(WebMediaPlayerImplTest, InfiniteDuration) {
  InitializeWebMediaPlayerImpl();
  SetDuration(kInfiniteDuration);

  // Send metadata so we have a watch time reporter created.
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  metadata.has_audio = true;
  metadata.audio_decoder_config = TestAudioConfig::Normal();
  metadata.natural_size = gfx::Size(400, 400);
  OnMetadata(metadata);

  EXPECT_EQ(std::numeric_limits<double>::infinity(), wmpi_->Duration());
  EXPECT_EQ(0, wmpi_->CurrentTime());
  EXPECT_EQ(base::TimeDelta(), GetCurrentTimeInternal());

  SetEnded(true);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), wmpi_->CurrentTime());
  EXPECT_EQ(base::TimeDelta(), GetCurrentTimeInternal());

  // Pause should not pick up infinity for the current time.
  wmpi_->Pause();
  EXPECT_EQ(std::numeric_limits<double>::infinity(), wmpi_->CurrentTime());
  EXPECT_EQ(base::TimeDelta(), GetCurrentTimeInternal());
}

// TODO(lethalantidote): Once |client_| is converted from a dummy to a mock,
// test that |web_layer| is actually used by |client_|.
// http://crbug/755880.
TEST_F(WebMediaPlayerImplTest, OnWebLayerReplacedGetsWebLayerFromBridge) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("UseSurfaceLayerForVideo", "");
  surface_layer_bridge_ = new StrictMock<MockSurfaceLayerBridge>();

  viz::FrameSinkId id = viz::FrameSinkId(1, 1);
  EXPECT_CALL(*surface_layer_bridge_, GetFrameSinkId()).WillOnce(ReturnRef(id));
  InitializeWebMediaPlayerImpl();

  std::unique_ptr<cc_blink::WebLayerImpl> web_layer =
      base::MakeUnique<cc_blink::WebLayerImpl>();

  EXPECT_CALL(*surface_layer_bridge_, GetWebLayer())
      .WillRepeatedly(Return(web_layer.get()));
  wmpi_->OnWebLayerReplaced();
}

TEST_F(WebMediaPlayerImplTest, PlaybackRateChangeMediaLogs) {
  InitializeWebMediaPlayerImpl();

  {
    InSequence s;

    // Expect precisely one rate change log from this test case.
    EXPECT_MEDIA_LOG_ON(*media_log_, PlaybackRateChanged("0", "0.8"));
    EXPECT_MEDIA_LOG_ON(*media_log_, WmpiDestroyed());

    wmpi_->SetRate(0.0);  // No change from initial rate, so no log.
    wmpi_->SetRate(0.8);  // This should log change from 0 -> 0.8
    wmpi_->SetRate(0.8);  // No change from previous rate, so no log.
  }
}

class WebMediaPlayerImplBackgroundBehaviorTest
    : public WebMediaPlayerImplTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, int, int, bool, bool, bool>> {
 public:
  // Indices of the tuple parameters.
  static const int kIsMediaSuspendEnabled = 0;
  static const int kIsBackgroundOptimizationEnabled = 1;
  static const int kDurationSec = 2;
  static const int kAverageKeyframeDistanceSec = 3;
  static const int kIsResumeBackgroundVideoEnabled = 4;
  static const int kIsMediaSource = 5;
  static const int kIsBackgroundPauseEnabled = 6;

  void SetUp() override {
    WebMediaPlayerImplTest::SetUp();
    SetUpMediaSuspend(IsMediaSuspendOn());

    std::string enabled_features;
    std::string disabled_features;
    if (IsBackgroundOptimizationOn()) {
      enabled_features += kBackgroundVideoTrackOptimization.name;
    } else {
      disabled_features += kBackgroundVideoTrackOptimization.name;
    }

    if (IsBackgroundPauseOn()) {
      if (!enabled_features.empty())
        enabled_features += ",";
      enabled_features += kBackgroundVideoPauseOptimization.name;
    } else {
      if (!disabled_features.empty())
        disabled_features += ",";
      disabled_features += kBackgroundVideoPauseOptimization.name;
    }

    if (IsResumeBackgroundVideoEnabled()) {
      if (!enabled_features.empty())
        enabled_features += ",";
      enabled_features += kResumeBackgroundVideo.name;
    } else {
      if (!disabled_features.empty())
        disabled_features += ",";
      disabled_features += kResumeBackgroundVideo.name;
    }

    feature_list_.InitFromCommandLine(enabled_features, disabled_features);

    InitializeWebMediaPlayerImpl();
    bool is_media_source = std::get<kIsMediaSource>(GetParam());
    SetLoadType(is_media_source ? blink::WebMediaPlayer::kLoadTypeMediaSource
                                : blink::WebMediaPlayer::kLoadTypeURL);
    SetVideoKeyframeDistanceAverage(
        base::TimeDelta::FromSeconds(GetAverageKeyframeDistanceSec()));
    SetDuration(base::TimeDelta::FromSeconds(GetDurationSec()));
    BackgroundPlayer();
  }

  void SetVideoKeyframeDistanceAverage(base::TimeDelta value) {
    PipelineStatistics statistics;
    statistics.video_keyframe_distance_average = value;
    wmpi_->SetPipelineStatisticsForTest(statistics);
  }

  bool IsMediaSuspendOn() {
    return std::get<kIsMediaSuspendEnabled>(GetParam());
  }

  bool IsBackgroundOptimizationOn() {
    return std::get<kIsBackgroundOptimizationEnabled>(GetParam());
  }

  bool IsResumeBackgroundVideoEnabled() {
    return std::get<kIsResumeBackgroundVideoEnabled>(GetParam());
  }

  bool IsBackgroundPauseOn() {
    return std::get<kIsBackgroundPauseEnabled>(GetParam());
  }

  int GetDurationSec() const { return std::get<kDurationSec>(GetParam()); }

  int GetAverageKeyframeDistanceSec() const {
    return std::get<kAverageKeyframeDistanceSec>(GetParam());
  }

  int GetMaxKeyframeDistanceSec() const {
    base::TimeDelta max_keyframe_distance =
        std::get<kIsMediaSource>(GetParam())
            ? kMaxKeyframeDistanceToDisableBackgroundVideoMSE
            : kMaxKeyframeDistanceToDisableBackgroundVideo;
    return max_keyframe_distance.InSeconds();
  }

  bool IsAndroid() {
#if defined(OS_ANDROID)
    return true;
#else
    return false;
#endif
  }

  bool ShouldDisableVideoWhenHidden() const {
    return wmpi_->ShouldDisableVideoWhenHidden();
  }

  bool ShouldPauseVideoWhenHidden() const {
    return wmpi_->ShouldPauseVideoWhenHidden();
  }

  bool IsBackgroundOptimizationCandidate() const {
    return wmpi_->IsBackgroundOptimizationCandidate();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioOnly) {
  // Never optimize or pause an audio-only player.
  SetMetadata(true, false);
  EXPECT_FALSE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, VideoOnly) {
  // Video only.
  SetMetadata(false, true);

  // Never disable video track for a video only stream.
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // Video only is always optimized.
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());

  // Video is always paused when suspension is on and only if matches the
  // optimization criteria if the optimization is on.
  bool should_pause = IsMediaSuspendOn() || IsBackgroundPauseOn();
  EXPECT_EQ(should_pause, ShouldPauseVideoWhenHidden());
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioVideo) {
  SetMetadata(true, true);

  // Optimization requirements are the same for all platforms.
  bool matches_requirements =
      (GetDurationSec() < GetMaxKeyframeDistanceSec()) ||
      (GetAverageKeyframeDistanceSec() < GetMaxKeyframeDistanceSec());

  EXPECT_EQ(matches_requirements, IsBackgroundOptimizationCandidate());
  EXPECT_EQ(IsBackgroundOptimizationOn() && matches_requirements,
            ShouldDisableVideoWhenHidden());

  // Only pause audible videos if both media suspend and resume background
  // videos is on. Both are on by default on Android and off on desktop.
  EXPECT_EQ(IsMediaSuspendOn() && IsResumeBackgroundVideoEnabled(),
            ShouldPauseVideoWhenHidden());

  if (!IsBackgroundOptimizationOn() || !matches_requirements ||
      !ShouldDisableVideoWhenHidden() || IsMediaSuspendOn()) {
    return;
  }

  // These tests start in background mode prior to having metadata, so put the
  // test back into a normal state.
  EXPECT_TRUE(IsDisableVideoTrackPending());

  ForegroundPlayer();
  EXPECT_FALSE(IsVideoTrackDisabled());
  EXPECT_FALSE(IsDisableVideoTrackPending());

  // Should start background disable timer, but not disable immediately.
  BackgroundPlayer();
  EXPECT_FALSE(IsVideoTrackDisabled());
  EXPECT_TRUE(IsDisableVideoTrackPending());
}

INSTANTIATE_TEST_CASE_P(BackgroundBehaviorTestInstances,
                        WebMediaPlayerImplBackgroundBehaviorTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool(),
                                           ::testing::Values(5, 300),
                                           ::testing::Values(5, 100),
                                           ::testing::Bool(),
                                           ::testing::Bool(),
                                           ::testing::Bool()));

}  // namespace media
