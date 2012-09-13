// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_web_peer_connection_00_handler_client.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPeerConnection00Handler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPeerConnectionHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"

class MediaStreamDependencyFactoryTest : public ::testing::Test {
 public:
  void SetUp() {
    dependency_factory_.reset(new MockMediaStreamDependencyFactory());
  }

  WebKit::WebMediaStreamDescriptor CreateWebKitMediaStream(bool audio,
                                                           bool video) {
    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
                      audio ? static_cast<size_t>(1) : 0);
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        video ? static_cast<size_t>(1) : 0);

    if (audio) {
      media_stream::StreamDeviceInfo info;
      info.stream_type = content::MEDIA_DEVICE_AUDIO_CAPTURE;
      info.name = "audio";
      info.session_id = 99;
      audio_sources[0].initialize("audio",
                                  WebKit::WebMediaStreamSource::TypeAudio,
                                  "audio");
      audio_sources[0].setExtraData(
              new MediaStreamSourceExtraData(info));
    }
    if (video) {
      media_stream::StreamDeviceInfo info;
      info.stream_type = content::MEDIA_DEVICE_VIDEO_CAPTURE;
      info.name = "video";
      info.session_id = 98;
      video_sources[0].initialize("video",
                                  WebKit::WebMediaStreamSource::TypeVideo,
                                  "video");
      video_sources[0].setExtraData(
              new MediaStreamSourceExtraData(info));
    }
    WebKit::WebMediaStreamDescriptor stream_desc;
    stream_desc.initialize("media stream", audio_sources, video_sources);

    return stream_desc;
  }

 protected:
  scoped_ptr<MockMediaStreamDependencyFactory> dependency_factory_;
};

TEST_F(MediaStreamDependencyFactoryTest, CreatePeerConnectionHandlerJsep) {
  WebKit::MockWebPeerConnection00HandlerClient client_jsep;
  scoped_ptr<WebKit::WebPeerConnection00Handler> pc_handler_jsep(
      dependency_factory_->CreatePeerConnectionHandlerJsep(&client_jsep));
  EXPECT_TRUE(pc_handler_jsep.get() != NULL);
}

TEST_F(MediaStreamDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  WebKit::MockWebRTCPeerConnectionHandlerClient client_jsep;
  scoped_ptr<WebKit::WebRTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(&client_jsep));
  EXPECT_TRUE(pc_handler.get() != NULL);
}

TEST_F(MediaStreamDependencyFactoryTest, CreateNativeMediaStream) {
  WebKit::WebMediaStreamDescriptor stream_desc = CreateWebKitMediaStream(true,
                                                                         true);
  EXPECT_TRUE(dependency_factory_->CreateNativeLocalMediaStream(&stream_desc));

  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      stream_desc.extraData());
  ASSERT_TRUE(extra_data && extra_data->local_stream());
  EXPECT_EQ(1u, extra_data->local_stream()->audio_tracks()->count());
  EXPECT_EQ(1u, extra_data->local_stream()->video_tracks()->count());
}

// Test that we don't crash if a MediaStream is created in WebKit with unknown
// sources. This can for example happen if a MediaStream is created with
// remote tracks.
TEST_F(MediaStreamDependencyFactoryTest, CreateNativeMediaStreamWithoutSource) {
  // Create a WebKit MediaStream description.
  WebKit::WebMediaStreamDescriptor stream_desc;
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

  EXPECT_TRUE(dependency_factory_->CreateNativeLocalMediaStream(&stream_desc));
  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      stream_desc.extraData());
  ASSERT_TRUE(extra_data && extra_data->local_stream());
  EXPECT_EQ(0u, extra_data->local_stream()->video_tracks()->count());
  EXPECT_EQ(0u, extra_data->local_stream()->audio_tracks()->count());
}
