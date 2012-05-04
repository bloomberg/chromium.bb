// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_web_peer_connection_00_handler_client.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "content/renderer/media/peer_connection_handler.h"
#include "content/renderer/media/peer_connection_handler_jsep.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "media/base/message_loop_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnection00Handler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

// TODO(perkj): Currently a media stream is identified by its sources.
// This is currently being changed in WebKit.
// Remove the creation of WebKitSourceVectors when that has landed.
static std::string CreateTrackLabel(
    const std::string& manager_label,
    int session_id,
    bool is_video) {
  std::string track_label = manager_label;
  if (is_video) {
    track_label += "#video-";
  } else {
    track_label += "#audio-";
  }
  track_label += session_id;
  return track_label;
}

// TODO(perkj): Currently a media stream is identified by its sources.
// This is currently being changed in WebKit.
// Remove the creation of WebKitSourceVectors when that has landed.
static void CreateWebKitSourceVector(
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& devices,
    WebKit::WebMediaStreamSource::Type type,
    WebKit::WebVector<WebKit::WebMediaStreamSource>& webkit_sources) {
  ASSERT(devices.size() == webkit_sources.size());

  for (size_t i = 0; i < devices.size(); ++i) {
    std::string track_label = CreateTrackLabel(
        label, devices[i].session_id,
        type == WebKit::WebMediaStreamSource::TypeVideo);
    webkit_sources[i].initialize(
          UTF8ToUTF16(track_label),
          type,
          UTF8ToUTF16(devices[i].name));
  }
}

class MediaStreamImplTest : public ::testing::Test {
 public:
  void SetUp() {
    // Create our test object.
    ms_dispatcher_.reset(new MockMediaStreamDispatcher());
    p2p_socket_dispatcher_.reset(new content::P2PSocketDispatcher(NULL));
    scoped_refptr<VideoCaptureImplManager> vc_manager(
        new VideoCaptureImplManager());
    MockMediaStreamDependencyFactory* dependency_factory =
        new MockMediaStreamDependencyFactory(vc_manager);
    ms_impl_.reset(new MediaStreamImpl(NULL,
                                       ms_dispatcher_.get(),
                                       p2p_socket_dispatcher_.get(),
                                       vc_manager.get(),
                                       dependency_factory));
  }

  void TearDown() {
    // Make sure the message created by
    // P2PSocketDispatcher::AsyncMessageSender::Send is handled before
    // tear down to avoid a memory leak.
    loop_.RunAllPending();
  }

  WebKit::WebMediaStreamDescriptor RequestLocalMediaStream(bool audio,
                                                           bool video) {
    WebKit::WebUserMediaRequest user_media_request;
    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
        audio ? static_cast<size_t>(1) : 0);
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        video ? static_cast<size_t>(1) : 0);
    ms_impl_->requestUserMedia(user_media_request, audio_sources,
                               video_sources);

    ms_impl_->OnStreamGenerated(ms_dispatcher_->request_id(),
                                ms_dispatcher_->stream_label(),
                                ms_dispatcher_->audio_array(),
                                ms_dispatcher_->video_array());

    // TODO(perkj): Currently a media stream is identified by its sources.
    // This is currently beeing changed in WebKit.
    // Remove the creation of WebKitSourceVectors when that has landed.
    if (audio) {
      CreateWebKitSourceVector(ms_dispatcher_->stream_label(),
                               ms_dispatcher_->audio_array(),
                               WebKit::WebMediaStreamSource::TypeAudio,
                               audio_sources);
    }
    if (video) {
      CreateWebKitSourceVector(ms_dispatcher_->stream_label(),
                               ms_dispatcher_->video_array(),
                               WebKit::WebMediaStreamSource::TypeVideo,
                               video_sources);
    }
    WebKit::WebMediaStreamDescriptor desc;
    desc.initialize(UTF8ToUTF16(ms_dispatcher_->stream_label()),
                    audio_sources, video_sources);
    return desc;
  }

 protected:
  MessageLoop loop_;
  scoped_ptr<MockMediaStreamDispatcher> ms_dispatcher_;
  scoped_ptr<content::P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_ptr<MediaStreamImpl> ms_impl_;
};

TEST_F(MediaStreamImplTest, Basic) {
  WebKit::MockWebPeerConnectionHandlerClient client;
  WebKit::WebPeerConnectionHandler* pc_handler =
      ms_impl_->CreatePeerConnectionHandler(&client);
  EXPECT_EQ(1u, ms_impl_->peer_connection_handlers_.size());

  // Delete PC handler explicitly after closing to mimic WebKit behavior.
  ms_impl_->ClosePeerConnection(
      static_cast<PeerConnectionHandler*>(pc_handler));
  EXPECT_TRUE(ms_impl_->peer_connection_handlers_.empty());
  delete pc_handler;

  WebKit::MockWebPeerConnection00HandlerClient client_jsep;
  WebKit::WebPeerConnection00Handler* pc_handler_jsep =
      ms_impl_->CreatePeerConnectionHandlerJsep(&client_jsep);
  EXPECT_EQ(1u, ms_impl_->peer_connection_handlers_.size());

  // Delete PC handler explicitly after closing to mimic WebKit behavior.
  ms_impl_->ClosePeerConnection(
      static_cast<PeerConnectionHandlerJsep*>(pc_handler_jsep));
  EXPECT_TRUE(ms_impl_->peer_connection_handlers_.empty());
  delete pc_handler_jsep;
}

TEST_F(MediaStreamImplTest, MultiplePeerConnections) {
  WebKit::MockWebPeerConnectionHandlerClient client;
  WebKit::WebPeerConnectionHandler* pc_handler =
      ms_impl_->CreatePeerConnectionHandler(&client);
  EXPECT_EQ(1u, ms_impl_->peer_connection_handlers_.size());

  WebKit::MockWebPeerConnection00HandlerClient client_jsep;
  WebKit::WebPeerConnection00Handler* pc_handler_jsep =
      ms_impl_->CreatePeerConnectionHandlerJsep(&client_jsep);
  EXPECT_EQ(2u, ms_impl_->peer_connection_handlers_.size());

  // Delete PC handler explicitly after closing to mimic WebKit behavior.
  ms_impl_->ClosePeerConnection(
      static_cast<PeerConnectionHandler*>(pc_handler));
  EXPECT_EQ(1u, ms_impl_->peer_connection_handlers_.size());
  delete pc_handler;

  // Delete PC handler explicitly after closing to mimic WebKit behavior.
  ms_impl_->ClosePeerConnection(
      static_cast<PeerConnectionHandlerJsep*>(pc_handler_jsep));
  EXPECT_TRUE(ms_impl_->peer_connection_handlers_.empty());
  delete pc_handler_jsep;
}

TEST_F(MediaStreamImplTest, LocalMediaStream) {
  // Test a stream with both audio and video.
  WebKit::WebMediaStreamDescriptor mixed_desc = RequestLocalMediaStream(true,
                                                                        true);
  webrtc::LocalMediaStreamInterface* mixed_stream =
      ms_impl_->GetLocalMediaStream(mixed_desc);
  ASSERT_TRUE(mixed_stream != NULL);
  EXPECT_EQ(1u, mixed_stream->audio_tracks()->count());
  EXPECT_EQ(1u, mixed_stream->video_tracks()->count());

  // Create a renderer for the stream
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactory());
  scoped_refptr<media::VideoDecoder> mixed_decoder(
      ms_impl_->CreateLocalVideoDecoder(mixed_stream,
                                        message_loop_factory.get()));
  EXPECT_TRUE(mixed_decoder.get() != NULL);

  // Test a stream with audio only.
  WebKit::WebMediaStreamDescriptor audio_desc = RequestLocalMediaStream(true,
                                                                        false);
  webrtc::LocalMediaStreamInterface* audio_stream =
      ms_impl_->GetLocalMediaStream(audio_desc);
  ASSERT_TRUE(audio_stream != NULL);
  EXPECT_EQ(1u, audio_stream->audio_tracks()->count());
  EXPECT_EQ(0u, audio_stream->video_tracks()->count());

  scoped_refptr<media::VideoDecoder> audio_decoder(
      ms_impl_->CreateLocalVideoDecoder(audio_stream,
                                        message_loop_factory.get()));
  EXPECT_TRUE(audio_decoder.get() == NULL);

  // Test a stream with video only.
  WebKit::WebMediaStreamDescriptor video_desc = RequestLocalMediaStream(false,
                                                                        true);
  webrtc::LocalMediaStreamInterface* video_stream_ =
      ms_impl_->GetLocalMediaStream(video_desc);
  ASSERT_TRUE(video_stream_ != NULL);
  EXPECT_EQ(0u, video_stream_->audio_tracks()->count());
  EXPECT_EQ(1u, video_stream_->video_tracks()->count());

  scoped_refptr<media::VideoDecoder> video_decoder(
      ms_impl_->CreateLocalVideoDecoder(video_stream_,
                                        message_loop_factory.get()));
  EXPECT_TRUE(video_decoder.get() != NULL);

  // Stop generated local streams.
  EXPECT_TRUE(ms_impl_->StopLocalMediaStream(mixed_desc));
  EXPECT_TRUE(ms_impl_->GetLocalMediaStream(mixed_desc) == NULL);

  EXPECT_TRUE(ms_impl_->StopLocalMediaStream(audio_desc));
  EXPECT_TRUE(ms_impl_->GetLocalMediaStream(audio_desc) == NULL);

  // Test that the MediaStreams are deleted if the owning WebFrame is deleted.
  // In the unit test the owning frame is NULL.
  ms_impl_->FrameWillClose(NULL);
  EXPECT_TRUE(ms_impl_->GetLocalMediaStream(video_desc) == NULL);
}
