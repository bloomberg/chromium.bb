// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(grunell): Fix issue 105556.
TEST(MediaStreamImplTest, DISABLED_Basic) {
  MessageLoop loop;

  // Create our test object.
  scoped_ptr<MockMediaStreamDispatcher> ms_dispatcher(
      new MockMediaStreamDispatcher());
  scoped_ptr<content::P2PSocketDispatcher> p2p_socket_dispatcher(
      new content::P2PSocketDispatcher(NULL));
  scoped_refptr<VideoCaptureImplManager> vc_manager(
      new VideoCaptureImplManager());
  MockMediaStreamDependencyFactory* dependency_factory =
      new MockMediaStreamDependencyFactory();
  scoped_refptr<MediaStreamImpl> ms_impl(new MediaStreamImpl(
      ms_dispatcher.get(),
      p2p_socket_dispatcher.get(),
      vc_manager.get(),
      dependency_factory));

  // TODO(grunell): Add tests for WebKit::WebUserMediaClient and
  // MediaStreamDispatcherEventHandler implementations.

  WebKit::MockWebPeerConnectionHandlerClient client;
  ms_impl->CreatePeerConnectionHandler(&client);
  EXPECT_TRUE(ms_impl->peer_connection_handler_);

  ms_impl->stream_labels_.push_back("label1");
  ms_impl->stream_labels_.push_back("label2");
  EXPECT_EQ(2, ms_dispatcher->stop_stream_counter());
  EXPECT_TRUE(ms_impl->stream_labels_.empty());

  ms_impl->ClosePeerConnection();
  EXPECT_FALSE(ms_impl->peer_connection_handler_);
}
