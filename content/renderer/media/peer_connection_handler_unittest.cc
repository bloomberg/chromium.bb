// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamSource.h"

TEST(PeerConnectionHandlerTest, Basic) {
  MessageLoop loop;

  scoped_ptr<WebKit::MockWebPeerConnectionHandlerClient> mock_client(
      new WebKit::MockWebPeerConnectionHandlerClient());
  scoped_refptr<MockMediaStreamImpl> mock_ms_impl(new MockMediaStreamImpl());
  MockMediaStreamDependencyFactory* mock_dependency_factory =
      new MockMediaStreamDependencyFactory();
  mock_dependency_factory->CreatePeerConnectionFactory(NULL, NULL, NULL);
  scoped_ptr<cricket::HttpPortAllocator> port_allocator(
      new cricket::HttpPortAllocator(NULL, "PeerConnection"));
  scoped_ptr<PeerConnectionHandler> pc_handler(
      new PeerConnectionHandler(mock_client.get(),
                                mock_ms_impl.get(),
                                mock_dependency_factory,
                                NULL,
                                port_allocator.get()));

  WebKit::WebString server_config(
      WebKit::WebString::fromUTF8("STUN stun.l.google.com:19302"));
  WebKit::WebSecurityOrigin security_origin;
  pc_handler->initialize(server_config, security_origin);
  EXPECT_TRUE(pc_handler->native_peer_connection_.get());
  webrtc::MockPeerConnectionImpl* mock_peer_connection =
      static_cast<webrtc::MockPeerConnectionImpl*>(
          pc_handler->native_peer_connection_.get());
  EXPECT_EQ(static_cast<webrtc::PeerConnectionObserver*>(pc_handler.get()),
            mock_peer_connection->observer());

  std::string label("label");
  WebKit::WebVector<WebKit::WebMediaStreamSource>
      source_vector(static_cast<size_t>(1));
  source_vector[0].initialize(WebKit::WebString::fromUTF8(label),
                              WebKit::WebMediaStreamSource::TypeVideo,
                              WebKit::WebString::fromUTF8("RemoteVideo"));
  WebKit::WebVector<WebKit::WebMediaStreamDescriptor>
      pendingAddStreams(static_cast<size_t>(1));
  pendingAddStreams[0].initialize(UTF8ToUTF16(label), source_vector);
  pc_handler->produceInitialOffer(pendingAddStreams);
  EXPECT_EQ(label, mock_ms_impl->video_label());
  EXPECT_EQ(label, mock_peer_connection->stream_id());
  EXPECT_TRUE(mock_peer_connection->video_stream());
  EXPECT_TRUE(mock_peer_connection->connected());
  EXPECT_TRUE(mock_peer_connection->video_capture_set());

  std::string message("message1");
  pc_handler->handleInitialOffer(WebKit::WebString::fromUTF8(message));
  EXPECT_EQ(message, mock_peer_connection->signaling_message());

  message = "message2";
  pc_handler->processSDP(WebKit::WebString::fromUTF8(message));
  EXPECT_EQ(message, mock_peer_connection->signaling_message());

  message = "message3";
  pc_handler->OnSignalingMessage(message);
  EXPECT_EQ(message, mock_client->sdp());

  std::string remote_label(label);
  remote_label.append("-remote");
  pc_handler->OnAddStream(remote_label, true);
  EXPECT_EQ(remote_label, mock_client->stream_label());

  RTCVideoDecoder* rtc_video_decoder = new RTCVideoDecoder(&loop, "");
  pc_handler->SetVideoRenderer(label, rtc_video_decoder);
  EXPECT_EQ(label, mock_peer_connection->video_renderer_stream_id());

  pc_handler->OnRemoveStream(remote_label, true);
  EXPECT_TRUE(mock_client->stream_label().empty());

  pc_handler->stop();
  EXPECT_FALSE(pc_handler->native_peer_connection_.get());
  // PC handler is expected to be deleted when stop calls
  // MediaStreamImpl::ClosePeerConnection. We own and delete it here instead of
  // in the mock.
  pc_handler.reset();

  // processPendingStreams must be tested on a new PC handler since removing
  // streams is currently not supported.
  pc_handler.reset(new PeerConnectionHandler(mock_client.get(),
                                             mock_ms_impl.get(),
                                             mock_dependency_factory,
                                             NULL,
                                             port_allocator.get()));
  pc_handler->initialize(server_config, security_origin);
  EXPECT_TRUE(pc_handler->native_peer_connection_.get());
  mock_peer_connection = static_cast<webrtc::MockPeerConnectionImpl*>(
      pc_handler->native_peer_connection_.get());

  WebKit::WebVector<WebKit::WebMediaStreamDescriptor>
      pendingRemoveStreams(static_cast<size_t>(0));
  pc_handler->processPendingStreams(pendingAddStreams, pendingRemoveStreams);
  EXPECT_EQ(label, mock_ms_impl->video_label());
  EXPECT_EQ(label, mock_peer_connection->stream_id());
  EXPECT_TRUE(mock_peer_connection->video_stream());
  EXPECT_TRUE(mock_peer_connection->connected());
  EXPECT_TRUE(mock_peer_connection->video_capture_set());

  pc_handler->stop();
  pc_handler.reset();
}
