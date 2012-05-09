// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_impl.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/peer_connection_handler.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"

namespace webrtc {

class MockVideoRendererWrapper : public VideoRendererWrapperInterface {
 public:
  virtual cricket::VideoRenderer* renderer() OVERRIDE { return NULL; }

 protected:
  virtual ~MockVideoRendererWrapper() {}
};

}  // namespace webrtc

TEST(PeerConnectionHandlerTest, WebMediaStreamDescriptorMemoryTest) {
  std::string stream_label("stream-label");
  std::string video_track_id("video-label");
  const size_t kSizeOne = 1;

  WebKit::WebMediaStreamSource source;
  source.initialize(WebKit::WebString::fromUTF8(video_track_id),
                    WebKit::WebMediaStreamSource::TypeVideo,
                    WebKit::WebString::fromUTF8("RemoteVideo"));

  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector(kSizeOne);
  source_vector[0] = source;

  WebKit::WebMediaStreamDescriptor local_stream;
  local_stream.initialize(UTF8ToUTF16(stream_label), source_vector);

  WebKit::WebMediaStreamDescriptor copy_1(local_stream);
  {
    WebKit::WebMediaStreamDescriptor copy_2(copy_1);
  }
}

TEST(PeerConnectionHandlerTest, Basic) {
  MessageLoop loop;

  scoped_ptr<WebKit::MockWebPeerConnectionHandlerClient> mock_client(
      new WebKit::MockWebPeerConnectionHandlerClient());
  scoped_ptr<MockMediaStreamImpl> mock_ms_impl(new MockMediaStreamImpl());
  scoped_ptr<MockMediaStreamDependencyFactory> mock_dependency_factory(
      new MockMediaStreamDependencyFactory(NULL));
  mock_dependency_factory->CreatePeerConnectionFactory(NULL,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       NULL);
  scoped_ptr<PeerConnectionHandler> pc_handler(
      new PeerConnectionHandler(mock_client.get(),
                                mock_ms_impl.get(),
                                mock_dependency_factory.get()));

  WebKit::WebString server_config(
      WebKit::WebString::fromUTF8("STUN stun.l.google.com:19302"));
  WebKit::WebString username;
  pc_handler->initialize(server_config, username);
  EXPECT_TRUE(pc_handler->native_peer_connection_.get());
  webrtc::MockPeerConnectionImpl* mock_peer_connection =
      static_cast<webrtc::MockPeerConnectionImpl*>(
          pc_handler->native_peer_connection_.get());

  // TODO(grunell): Add an audio track as well.
  std::string stream_label("stream-label");
  std::string video_track_label("video-label");
  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> local_stream(
      mock_dependency_factory->CreateLocalMediaStream(stream_label));
  talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> local_video_track(
      mock_dependency_factory->CreateLocalVideoTrack(video_track_label, 0));
  local_stream->AddTrack(local_video_track);
  mock_ms_impl->AddLocalStream(local_stream);
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector(
      static_cast<size_t>(1));
  source_vector[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                              WebKit::WebMediaStreamSource::TypeVideo,
                              WebKit::WebString::fromUTF8("RemoteVideo"));
  WebKit::WebVector<WebKit::WebMediaStreamDescriptor> local_streams(
      static_cast<size_t>(1));
  local_streams[0].initialize(UTF8ToUTF16(stream_label), source_vector);
  pc_handler->produceInitialOffer(local_streams);
  EXPECT_EQ(stream_label, mock_peer_connection->stream_label());
  EXPECT_TRUE(mock_peer_connection->stream_changes_committed());

  std::string message("message1");
  pc_handler->handleInitialOffer(WebKit::WebString::fromUTF8(message));
  EXPECT_EQ(message, mock_peer_connection->signaling_message());

  message = "message2";
  pc_handler->processSDP(WebKit::WebString::fromUTF8(message));
  EXPECT_EQ(message, mock_peer_connection->signaling_message());

  message = "message3";
  pc_handler->OnSignalingMessage(message);
  EXPECT_EQ(message, mock_client->sdp());

  std::string remote_stream_label(stream_label);
  remote_stream_label += "-remote";
  std::string remote_video_track_label(video_track_label);
  remote_video_track_label += "-remote";
  // We use a local stream as a remote since for testing purposes we really
  // only need the MediaStreamInterface.
  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> remote_stream(
      mock_dependency_factory->CreateLocalMediaStream(remote_stream_label));
  talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> remote_video_track(
      mock_dependency_factory->CreateLocalVideoTrack(remote_video_track_label,
                                                     0));
  remote_video_track->set_enabled(true);
  remote_stream->AddTrack(remote_video_track);
  mock_peer_connection->AddRemoteStream(remote_stream);
  pc_handler->OnAddStream(remote_stream);
  EXPECT_EQ(remote_stream_label, mock_client->stream_label());

  talk_base::scoped_refptr<webrtc::MockVideoRendererWrapper> renderer(
      new talk_base::RefCountedObject<webrtc::MockVideoRendererWrapper>());
  pc_handler->SetRemoteVideoRenderer(remote_video_track_label, renderer);
  EXPECT_EQ(renderer, remote_video_track->GetRenderer());

  WebKit::WebVector<WebKit::WebMediaStreamDescriptor> empty_streams(
      static_cast<size_t>(0));
  pc_handler->processPendingStreams(empty_streams, local_streams);
  EXPECT_EQ("", mock_peer_connection->stream_label());
  mock_peer_connection->ClearStreamChangesCommitted();
  EXPECT_TRUE(!mock_peer_connection->stream_changes_committed());

  pc_handler->OnRemoveStream(remote_stream);
  EXPECT_TRUE(mock_client->stream_label().empty());

  pc_handler->processPendingStreams(local_streams, empty_streams);
  EXPECT_EQ(stream_label, mock_peer_connection->stream_label());
  EXPECT_TRUE(mock_peer_connection->stream_changes_committed());

  pc_handler->stop();
  EXPECT_FALSE(pc_handler->native_peer_connection_.get());
  // PC handler is expected to be deleted when stop calls
  // MediaStreamImpl::ClosePeerConnection. We own and delete it here instead of
  // in the mock.
  pc_handler.reset();
}
