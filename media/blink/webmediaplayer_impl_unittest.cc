// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media_log.h"
#include "media/base/test_helpers.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_impl.h"
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

// Dummy superclass necessary since blink::WebFrameClient() has a protected
// destructor.
class DummyWebFrameClient : public blink::WebFrameClient {};

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
        make_scoped_ptr(new DefaultRendererFactory(
            media_log_, nullptr, DefaultRendererFactory::GetGpuFactoriesCB(),
            audio_hardware_config_)),
        url_index_,
        WebMediaPlayerParams(
            WebMediaPlayerParams::DeferLoadCB(),
            scoped_refptr<RestartableAudioRendererSink>(), media_log_,
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
  // "Renderer" thread.
  base::MessageLoop message_loop_;

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  DummyWebFrameClient web_frame_client_;
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
  scoped_ptr<WebMediaPlayerImpl> wmpi_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {}

}  // namespace media
