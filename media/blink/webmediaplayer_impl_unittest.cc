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
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/test_helpers.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/renderers/default_renderer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

int64_t OnAdjustAllocatedMemory(int64_t delta) {
  return 0;
}

class DummyWebMediaPlayerClient : public blink::WebMediaPlayerClient {
 public:
  DummyWebMediaPlayerClient() {}

  // blink::WebMediaPlayerClient implementation.
  void networkStateChanged() override {}
  void readyStateChanged() override {}
  void timeChanged() override {}
  void repaint() override {}
  void durationChanged() override {}
  void sizeChanged() override {}
  void playbackStateChanged() override {}
  void setWebLayer(blink::WebLayer*) override {}
  blink::WebMediaPlayer::TrackId addAudioTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::AudioTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool enabled) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void removeAudioTrack(blink::WebMediaPlayer::TrackId) override {}
  blink::WebMediaPlayer::TrackId addVideoTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::VideoTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool selected) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void removeVideoTrack(blink::WebMediaPlayer::TrackId) override {}
  void addTextTrack(blink::WebInbandTextTrack*) override {}
  void removeTextTrack(blink::WebInbandTextTrack*) override {}
  void mediaSourceOpened(blink::WebMediaSource*) override {}
  void requestSeek(double) override {}
  void remoteRouteAvailabilityChanged(
      blink::WebRemotePlaybackAvailability) override {}
  void connectedToRemoteDevice() override {}
  void disconnectedFromRemoteDevice() override {}
  void cancelledRemotePlaybackRequest() override {}
  void remotePlaybackStarted() override {}
  bool isAutoplayingMuted() override { return is_autoplaying_muted_; }
  void requestReload(const blink::WebURL& newUrl) override {}
  bool hasSelectedVideoTrack() override { return false; }
  blink::WebMediaPlayer::TrackId getSelectedVideoTrackId() override {
    return blink::WebMediaPlayer::TrackId();
  }

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
  MOCK_METHOD0(IsBackgroundVideoPlaybackUnlocked, bool());

  void SetIdle(int player_id, bool is_idle) override {
    DCHECK_EQ(player_id_, player_id);
    is_idle_ = is_idle;
    is_stale_ &= is_idle;
  }

  bool IsIdle(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_idle_;
  }

  void ClearStaleFlag(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    is_stale_ = false;
  }

  bool IsStale(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_stale_;
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

class WebMediaPlayerImplTest : public testing::Test {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        web_view_(blink::WebView::create(nullptr,
                                         blink::WebPageVisibilityStateVisible)),
        web_local_frame_(
            blink::WebLocalFrame::create(blink::WebTreeScopeType::Document,
                                         &web_frame_client_)),
        media_log_(new MediaLog()),
        audio_parameters_(TestAudioParameters::Normal()) {
    web_view_->setMainFrame(web_local_frame_);
    media_thread_.StartAndWaitForTesting();
  }

  void InitializeWebMediaPlayerImpl() {
    wmpi_.reset(new WebMediaPlayerImpl(
        web_local_frame_, &client_, nullptr, &delegate_,
        base::MakeUnique<DefaultRendererFactory>(
            media_log_, nullptr, DefaultRendererFactory::GetGpuFactoriesCB()),
        url_index_,
        WebMediaPlayerParams(
            WebMediaPlayerParams::DeferLoadCB(),
            scoped_refptr<SwitchableAudioRendererSink>(), media_log_,
            media_thread_.task_runner(), message_loop_.task_runner(),
            message_loop_.task_runner(), WebMediaPlayerParams::Context3DCB(),
            base::Bind(&OnAdjustAllocatedMemory), nullptr, nullptr, nullptr,
            base::TimeDelta::FromSeconds(10))));
  }

  ~WebMediaPlayerImplTest() override {
    // Destruct WebMediaPlayerImpl and pump the message loop to ensure that
    // objects passed to the message loop for destruction are released.
    //
    // NOTE: This should be done before any other member variables are
    // destructed since WMPI may reference them during destruction.
    wmpi_.reset();
    base::RunLoop().RunUntilIdle();

    web_view_->close();
  }

 protected:
  void SetReadyState(blink::WebMediaPlayer::ReadyState state) {
    wmpi_->SetReadyState(state);
  }

  void SetPaused(bool is_paused) { wmpi_->paused_ = is_paused; }
  void SetSeeking(bool is_seeking) { wmpi_->seeking_ = is_seeking; }
  void SetEnded(bool is_ended) { wmpi_->ended_ = is_ended; }
  void SetTickClock(base::TickClock* clock) { wmpi_->tick_clock_.reset(clock); }

  void SetFullscreen(bool is_fullscreen) {
    wmpi_->overlay_enabled_ = is_fullscreen;
  }

  void SetMetadata(bool has_audio, bool has_video) {
    wmpi_->SetNetworkState(blink::WebMediaPlayer::NetworkStateLoaded);
    wmpi_->SetReadyState(blink::WebMediaPlayer::ReadyStateHaveMetadata);
    wmpi_->pipeline_metadata_.has_audio = has_audio;
    wmpi_->pipeline_metadata_.has_video = has_video;
  }

  void OnMetadata(PipelineMetadata metadata) { wmpi_->OnMetadata(metadata); }

  void OnVideoNaturalSizeChange(const gfx::Size& size) {
    wmpi_->OnVideoNaturalSizeChange(size);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_FrameHidden() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, true);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Suspended() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, true, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Remote() {
    return wmpi_->UpdatePlayState_ComputePlayState(true, false, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_BackgroundedStreaming() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true);
  }

  bool IsSuspended() { return wmpi_->pipeline_controller_.IsSuspended(); }

  void AddBufferedRanges() {
    wmpi_->buffered_data_source_host_.AddBufferedByteRange(0, 1);
  }

  void SetDelegateState(WebMediaPlayerImpl::DelegateState state) {
    wmpi_->SetDelegateState(state, false);
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

  void SetVideoKeyframeDistanceAverage(base::TimeDelta value) {
    PipelineStatistics statistics;
    statistics.video_keyframe_distance_average = value;
    wmpi_->SetPipelineStatisticsForTest(statistics);
  }

  void SetDuration(base::TimeDelta value) {
    wmpi_->SetPipelineMediaDurationForTest(value);
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

  scoped_refptr<MediaLog> media_log_;
  linked_ptr<media::UrlIndex> url_index_;

  // Audio hardware configuration.
  AudioParameters audio_parameters_;

  // The client interface used by |wmpi_|. Just a dummy for now, but later we
  // may want a mock or intelligent fake.
  DummyWebMediaPlayerClient client_;

  testing::NiceMock<MockWebMediaPlayerDelegate> delegate_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledBeforeLoadingBegins) {
  InitializeWebMediaPlayerImpl();
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest,
       IdleSuspendIsDisabledIfLoadingProgressedRecently) {
  InitializeWebMediaPlayerImpl();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(clock);
  AddBufferedRanges();
  wmpi_->didLoadingProgress();
  // Advance less than the loading timeout.
  clock->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledIfLoadingHasStalled) {
  InitializeWebMediaPlayerImpl();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(clock);
  AddBufferedRanges();
  wmpi_->didLoadingProgress();
  // Advance more than the loading timeout.
  clock->Advance(base::TimeDelta::FromSeconds(4));
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, DidLoadingProgressTriggersResume) {
  // Same setup as IdleSuspendIsEnabledBeforeLoadingBegins.
  InitializeWebMediaPlayerImpl();
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());

  // Like IdleSuspendIsDisabledIfLoadingProgressedRecently, the idle timeout
  // should be rejected if it hasn't been long enough.
  AddBufferedRanges();
  wmpi_->didLoadingProgress();
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveCurrentData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHidden) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kResumeBackgroundVideo);

    WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
    EXPECT_TRUE(state.is_idle);
    EXPECT_TRUE(state.is_suspended);
    EXPECT_FALSE(state.is_memory_reporting_enabled);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kResumeBackgroundVideo);

    WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
    EXPECT_TRUE(state.is_idle);
    EXPECT_TRUE(state.is_suspended);
    EXPECT_FALSE(state.is_memory_reporting_enabled);
  }
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameClosed) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PlayingBackgroundedVideo) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);
  EXPECT_CALL(delegate_, IsBackgroundVideoPlaybackUnlocked())
      .WillRepeatedly(Return(true));

  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AudioOnly) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, false);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  // Backgrounded audio-only playback stays playing.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_StartsAndStops) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
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
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);
  client_.set_is_autoplaying_muted(true);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, _));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  client_.set_is_autoplaying_muted(false);
  EXPECT_CALL(delegate_, DidPlay(_, true, true, _));
  wmpi_->setVolume(1.0);
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.natural_size = gfx::Size(320, 240);

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->naturalSize());

  // TODO(sandersd): Verify that the client is notified of the size change?
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1920, 1080), wmpi_->naturalSize());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange_Rotated) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.natural_size = gfx::Size(320, 240);
  metadata.video_rotation = VIDEO_ROTATION_90;

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->naturalSize());

  // For 90/270deg rotations, the natural size should be transposed.
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1080, 1920), wmpi_->naturalSize());
}

TEST_F(WebMediaPlayerImplTest, BackgroundOptimizationsFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBackgroundVideoTrackOptimization);
  InitializeWebMediaPlayerImpl();
  SetVideoKeyframeDistanceAverage(base::TimeDelta::FromSeconds(5));
  SetDuration(base::TimeDelta::FromSeconds(300));

  // Audible video.
  SetMetadata(true, true);
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_TRUE(ShouldDisableVideoWhenHidden());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());

  // Video only.
  SetMetadata(false, true);
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_TRUE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // Audio only.
  SetMetadata(true, false);
  EXPECT_FALSE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // Duration is shorter than max video keyframe distance.
  SetDuration(base::TimeDelta::FromSeconds(5));
  SetMetadata(true, true);
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_TRUE(ShouldDisableVideoWhenHidden());

  // Average keyframe distance is too big.
  SetVideoKeyframeDistanceAverage(base::TimeDelta::FromSeconds(100));
  SetDuration(base::TimeDelta::FromSeconds(300));
  EXPECT_FALSE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
}

TEST_F(WebMediaPlayerImplTest, BackgroundOptimizationsFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kBackgroundVideoTrackOptimization);

  InitializeWebMediaPlayerImpl();
  SetVideoKeyframeDistanceAverage(base::TimeDelta::FromSeconds(5));
  SetDuration(base::TimeDelta::FromSeconds(300));

  // Audible video.
  SetMetadata(true, true);
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());

  // Video only (pausing is enabled on Android).
  SetMetadata(false, true);
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
#if defined(OS_ANDROID)
  EXPECT_TRUE(ShouldPauseVideoWhenHidden());

  // On Android, the duration and keyframe distance don't matter for video-only.
  SetDuration(base::TimeDelta::FromSeconds(5));
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_TRUE(ShouldPauseVideoWhenHidden());

  SetVideoKeyframeDistanceAverage(base::TimeDelta::FromSeconds(100));
  SetDuration(base::TimeDelta::FromSeconds(300));
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_TRUE(ShouldPauseVideoWhenHidden());

  // Restore average keyframe distance.
  SetVideoKeyframeDistanceAverage(base::TimeDelta::FromSeconds(5));
#else
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
#endif

  // Audio only.
  SetMetadata(true, false);
  EXPECT_FALSE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // Duration is shorter than max video keyframe distance.
  SetDuration(base::TimeDelta::FromSeconds(5));
  SetMetadata(true, true);
  EXPECT_TRUE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // Average keyframe distance is too big.
  SetVideoKeyframeDistanceAverage(base::TimeDelta::FromSeconds(100));
  SetDuration(base::TimeDelta::FromSeconds(300));
  EXPECT_FALSE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
}

}  // namespace media
