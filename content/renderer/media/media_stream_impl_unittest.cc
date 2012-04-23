// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_web_peer_connection_00_handler_client.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "content/renderer/media/peer_connection_handler.h"
#include "content/renderer/media/peer_connection_handler_jsep.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnection00Handler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandler.h"

class MediaStreamImplTest : public ::testing::Test {
 public:
  void SetUp() {
    // Create our test object.
    ms_dispatcher_.reset(new MockMediaStreamDispatcher());
    p2p_socket_dispatcher_.reset(new content::P2PSocketDispatcher(NULL));
    scoped_refptr<VideoCaptureImplManager> vc_manager(
        new VideoCaptureImplManager());
    MockMediaStreamDependencyFactory* dependency_factory =
        new MockMediaStreamDependencyFactory();
    ms_impl_.reset(new MediaStreamImpl(NULL,
                                       ms_dispatcher_.get(),
                                       p2p_socket_dispatcher_.get(),
                                       vc_manager.get(),
                                       dependency_factory));
  }

 protected:
  MessageLoop loop_;
  scoped_ptr<MockMediaStreamDispatcher> ms_dispatcher_;
  scoped_ptr<content::P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_ptr<MediaStreamImpl> ms_impl_;
};

TEST_F(MediaStreamImplTest, Basic) {
  // TODO(grunell): Add tests for WebKit::WebUserMediaClient and
  // MediaStreamDispatcherEventHandler implementations.

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
  // TODO(grunell): Add tests for WebKit::WebUserMediaClient and
  // MediaStreamDispatcherEventHandler implementations.

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
