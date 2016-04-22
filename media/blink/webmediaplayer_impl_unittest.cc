// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media_log.h"
#include "media/base/test_helpers.h"
#include "media/blink/mock_webframeclient.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/renderers/default_renderer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

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
    return 0;
  }
  void removeAudioTrack(blink::WebMediaPlayer::TrackId) override {}
  blink::WebMediaPlayer::TrackId addVideoTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::VideoTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool selected) override {
    return 0;
  }
  void removeVideoTrack(blink::WebMediaPlayer::TrackId) override {}
  void addTextTrack(blink::WebInbandTextTrack*) override {}
  void removeTextTrack(blink::WebInbandTextTrack*) override {}
  void mediaSourceOpened(blink::WebMediaSource*) override {}
  void requestSeek(double) override {}
  void remoteRouteAvailabilityChanged(bool) override {}
  void connectedToRemoteDevice() override {}
  void disconnectedFromRemoteDevice() override {}
  void cancelledRemotePlaybackRequest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyWebMediaPlayerClient);
};

class WebMediaPlayerImplTest : public testing::Test {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        web_view_(blink::WebView::create(nullptr)),
        web_local_frame_(
            blink::WebLocalFrame::create(blink::WebTreeScopeType::Document,
                                         &web_frame_client_)),
        media_log_(new MediaLog()),
        audio_parameters_(TestAudioParameters::Normal()),
        audio_hardware_config_(audio_parameters_, audio_parameters_) {
    web_view_->setMainFrame(web_local_frame_);
    media_thread_.StartAndWaitForTesting();

    wmpi_.reset(new WebMediaPlayerImpl(
        web_local_frame_, &client_, nullptr,
        base::WeakPtr<WebMediaPlayerDelegate>(),
        base::WrapUnique(new DefaultRendererFactory(
            media_log_, nullptr, DefaultRendererFactory::GetGpuFactoriesCB(),
            audio_hardware_config_)),
        url_index_,
        WebMediaPlayerParams(
            WebMediaPlayerParams::DeferLoadCB(),
            scoped_refptr<SwitchableAudioRendererSink>(), media_log_,
            media_thread_.task_runner(), message_loop_.task_runner(),
            message_loop_.task_runner(), WebMediaPlayerParams::Context3DCB(),
            base::Bind(&OnAdjustAllocatedMemory), nullptr, nullptr, nullptr)));
  }

  ~WebMediaPlayerImplTest() override {
    // Destruct WebMediaPlayerImpl and pump the message loop to ensure that
    // objects passed to the message loop for destruction are released.
    //
    // NOTE: This should be done before any other member variables are
    // destructed since WMPI may reference them during destruction.
    wmpi_.reset();
    message_loop_.RunUntilIdle();

    web_view_->close();
    web_local_frame_->close();
  }

 protected:
  void SetReadyState(blink::WebMediaPlayer::ReadyState state) {
    wmpi_->SetReadyState(state);
  }

  void SetPaused(bool is_paused) { wmpi_->paused_ = is_paused; }
  void SetSeeking(bool is_seeking) { wmpi_->seeking_ = is_seeking; }
  void SetEnded(bool is_ended) { wmpi_->ended_ = is_ended; }
  void SetFullscreen(bool is_fullscreen) { wmpi_->fullscreen_ = is_fullscreen; }

  void SetMetadata(bool has_audio, bool has_video) {
    wmpi_->SetNetworkState(blink::WebMediaPlayer::NetworkStateLoaded);
    wmpi_->SetReadyState(blink::WebMediaPlayer::ReadyStateHaveMetadata);
    wmpi_->pipeline_metadata_.has_audio = has_audio;
    wmpi_->pipeline_metadata_.has_video = has_video;
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false);
  }

  WebMediaPlayerImpl::PlayState ComputeBackgroundedPlayState() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, true);
  }

  WebMediaPlayerImpl::PlayState ComputeIdlePlayState() {
    wmpi_->is_idle_ = true;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false);
  }

  WebMediaPlayerImpl::PlayState ComputeMustSuspendPlayState() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = true;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false);
  }

  // "Renderer" thread.
  base::MessageLoop message_loop_;

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  MockWebFrameClient web_frame_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;

  scoped_refptr<MediaLog> media_log_;
  linked_ptr<media::UrlIndex> url_index_;

  // Audio hardware configuration.
  AudioParameters audio_parameters_;
  AudioHardwareConfig audio_hardware_config_;

  // The client interface used by |wmpi_|. Just a dummy for now, but later we
  // may want a mock or intelligent fake.
  DummyWebMediaPlayerClient client_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterConstruction) {
  WebMediaPlayerImpl::PlayState state;

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterMetadata) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterMetadata_AudioOnly) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, false);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Background suspend is not enabled for audio-only.
  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterFutureData) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  // Idle suspension is possible after HaveFutureData.
  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing_AudioOnly) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, false);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Audio-only stays playing in the background.
  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Paused_Seek) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetSeeking(true);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED_BUT_NOT_IDLE,
            state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Paused_Fullscreen) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetFullscreen(true);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED_BUT_NOT_IDLE,
            state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Ended) {
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetEnded(true);

  // The pipeline is not suspended immediately on ended.
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::ENDED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::ENDED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

}  // namespace media
