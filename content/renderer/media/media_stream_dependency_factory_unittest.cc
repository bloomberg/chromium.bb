// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace content {

class MediaSourceCreatedObserver {
 public:
  MediaSourceCreatedObserver()
     : result_(false),
       description_(NULL) {
  }

  void OnCreateNativeSourcesComplete(
      WebKit::WebMediaStream* description,
      bool request_succeeded) {
    result_ = request_succeeded;
    description_ = description;
  }

  WebKit::WebMediaStream* description() const {
    return description_;
  }
  bool result() const { return result_; }

 private:
  bool result_;
  WebKit::WebMediaStream* description_;
};

class MediaStreamDependencyFactoryTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    dependency_factory_.reset(new MockMediaStreamDependencyFactory());
  }

  WebKit::WebMediaStream CreateWebKitMediaStream(bool audio,
                                                           bool video) {
    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
                      audio ? static_cast<size_t>(1) : 0);
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        video ? static_cast<size_t>(1) : 0);

    if (audio) {
      StreamDeviceInfo info;
      info.device.type = content::MEDIA_DEVICE_AUDIO_CAPTURE;
      info.device.name = "audio";
      info.device.sample_rate = 0;
      info.device.channel_layout = 0;
      info.session_id = 99;
      audio_sources[0].initialize("audio",
                                  WebKit::WebMediaStreamSource::TypeAudio,
                                  "audio");
      audio_sources[0].setExtraData(
              new MediaStreamSourceExtraData(info));
    }
    if (video) {
      StreamDeviceInfo info;
      info.device.type = content::MEDIA_DEVICE_VIDEO_CAPTURE;
      info.device.name = "video";
      info.session_id = 98;
      video_sources[0].initialize("video",
                                  WebKit::WebMediaStreamSource::TypeVideo,
                                  "video");
      video_sources[0].setExtraData(
              new MediaStreamSourceExtraData(info));
    }
    WebKit::WebMediaStream stream_desc;
    stream_desc.initialize("media stream", audio_sources, video_sources);

    return stream_desc;
  }

  void CreateNativeSources(WebKit::WebMediaStream* descriptor) {
    MediaSourceCreatedObserver observer;
    WebKit::WebMediaConstraints audio_constraints;
    dependency_factory_->CreateNativeMediaSources(
        WebKit::WebMediaConstraints(),
        WebKit::WebMediaConstraints(),
        descriptor,
        base::Bind(
            &MediaSourceCreatedObserver::OnCreateNativeSourcesComplete,
            base::Unretained(&observer)));

    EXPECT_FALSE(observer.result());
    // Change the state of the created source to live. This should trigger
    // MediaSourceCreatedObserver::OnCreateNativeSourcesComplete
    if (dependency_factory_->last_video_source()) {
      dependency_factory_->last_audio_source()->SetLive();
      dependency_factory_->last_video_source()->SetLive();
    }
    EXPECT_TRUE(observer.result());
    EXPECT_TRUE(observer.description() == descriptor);
  }

 protected:
  scoped_ptr<MockMediaStreamDependencyFactory> dependency_factory_;
};

TEST_F(MediaStreamDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  MockWebRTCPeerConnectionHandlerClient client_jsep;
  scoped_ptr<WebKit::WebRTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(&client_jsep));
  EXPECT_TRUE(pc_handler.get() != NULL);
}

TEST_F(MediaStreamDependencyFactoryTest, CreateNativeMediaStream) {
  WebKit::WebMediaStream stream_desc = CreateWebKitMediaStream(true, true);
  CreateNativeSources(&stream_desc);

  dependency_factory_->CreateNativeLocalMediaStream(&stream_desc);

  content::MediaStreamExtraData* extra_data =
      static_cast<content::MediaStreamExtraData*>(stream_desc.extraData());
  ASSERT_TRUE(extra_data && extra_data->stream());
  EXPECT_TRUE(extra_data->is_local());
  EXPECT_EQ(1u, extra_data->stream()->GetAudioTracks().size());
  EXPECT_EQ(1u, extra_data->stream()->GetVideoTracks().size());
}

// Test that we don't crash if a MediaStream is created in WebKit with unknown
// sources. This can for example happen if a MediaStream is created with
// remote tracks.
TEST_F(MediaStreamDependencyFactoryTest, CreateNativeMediaStreamWithoutSource) {
  // Create a WebKit MediaStream description.
  WebKit::WebMediaStream stream_desc;
  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
      static_cast<size_t>(1));
  audio_sources[0].initialize("audio source",
                              WebKit::WebMediaStreamSource::TypeAudio,
                              "something");
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
      static_cast<size_t>(1));
  video_sources[0].initialize("video source",
                              WebKit::WebMediaStreamSource::TypeVideo,
                              "something");
  stream_desc.initialize("new stream", audio_sources, video_sources);

  EXPECT_TRUE(dependency_factory_->EnsurePeerConnectionFactory());
  dependency_factory_->CreateNativeLocalMediaStream(&stream_desc);
  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      stream_desc.extraData());
  ASSERT_TRUE(extra_data && extra_data->stream());
  EXPECT_TRUE(extra_data->is_local());
  EXPECT_EQ(0u, extra_data->stream()->GetVideoTracks().size());
  EXPECT_EQ(0u, extra_data->stream()->GetAudioTracks().size());
}

}  // namespace content
