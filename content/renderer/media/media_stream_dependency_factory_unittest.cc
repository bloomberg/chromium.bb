// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"

namespace content {

class MediaStreamDependencyFactoryTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    dependency_factory_.reset(new MockMediaStreamDependencyFactory());
  }

 protected:
  scoped_ptr<MockMediaStreamDependencyFactory> dependency_factory_;
};

TEST_F(MediaStreamDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  MockWebRTCPeerConnectionHandlerClient client_jsep;
  scoped_ptr<blink::WebRTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(&client_jsep));
  EXPECT_TRUE(pc_handler.get() != NULL);
}

}  // namespace content
