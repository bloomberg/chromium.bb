// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/mock_web_rtc_peer_connection_handler_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

class PeerConnectionDependencyFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
};

TEST_F(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  MockWebRTCPeerConnectionHandlerClient client_jsep;
  std::unique_ptr<blink::WebRTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(
          &client_jsep,
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  EXPECT_TRUE(pc_handler.get() != nullptr);
}

}  // namespace content
