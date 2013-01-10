// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class MockWebRTCInternalsProxy : public content::WebRTCInternalsUIObserver {
 public:
  void OnUpdate(const std::string& command, const Value* value) OVERRIDE {
    data_ = command;
  }

  std::string data() {
    return data_;
  }

 private:
   std::string data_;
};

class WebRTCInternalsTest : public testing::Test {
 public:
  WebRTCInternalsTest()
      : io_thread_(content::BrowserThread::IO, &io_loop_) {}

 protected:
  virtual void SetUp() {
    webrtc_internals_ = WebRTCInternals::GetInstance();
  }

  PeerConnectionInfo GetPeerConnectionInfo(uintptr_t lid) {
    PeerConnectionInfo info;
    info.lid = lid;
    info.servers = "s";
    info.constraints = "c";
    info.url = "u";
    return info;
  }
  std::string ExpectedInfo(std::string prefix,
                           std::string id,
                           std::string suffix) {
    static const std::string  kstatic_part1 = std::string(
        "{\"constraints\":\"c\",");
    static const std::string kstatic_part2 = std::string(
        ",\"servers\":\"s\",\"url\":\"u\"}");
    return prefix + kstatic_part1 + id + kstatic_part2 + suffix;
  }

  MessageLoop io_loop_;
  content::TestBrowserThread io_thread_;
  WebRTCInternals *webrtc_internals_;
};

TEST_F(WebRTCInternalsTest, GetInstance) {
  EXPECT_TRUE(webrtc_internals_);
}

TEST_F(WebRTCInternalsTest, AddRemoveObserver) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->RemoveObserver(observer.get());
  webrtc_internals_->AddPeerConnection(3, GetPeerConnectionInfo(4));
  EXPECT_EQ("", observer->data());

  webrtc_internals_->RemovePeerConnection(3, 4);
}

TEST_F(WebRTCInternalsTest, SendAddPeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, GetPeerConnectionInfo(2));
  EXPECT_EQ("updatePeerConnectionAdded", observer->data());

  webrtc_internals_->RemoveObserver(observer.get());
  webrtc_internals_->RemovePeerConnection(1, 2);
}

TEST_F(WebRTCInternalsTest, SendRemovePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, GetPeerConnectionInfo(2));
  webrtc_internals_->RemovePeerConnection(1, 2);
  EXPECT_EQ("updatePeerConnectionRemoved", observer->data());

  webrtc_internals_->RemoveObserver(observer.get());
}

}  // namespace content
