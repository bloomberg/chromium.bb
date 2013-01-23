// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/media/chrome_webrtc_internals.h"
#include "chrome/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

static const std::string kContraints = "c";
static const std::string kServers = "s";
static const std::string kUrl = "u";

class MockWebRTCInternalsProxy : public WebRTCInternalsUIObserver {
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
      : io_thread_(content::BrowserThread::UI, &io_loop_) {}

 protected:
  virtual void SetUp() {
    webrtc_internals_ = ChromeWebRTCInternals::GetInstance();
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
  ChromeWebRTCInternals *webrtc_internals_;
};

TEST_F(WebRTCInternalsTest, GetInstance) {
  EXPECT_TRUE(webrtc_internals_);
}

TEST_F(WebRTCInternalsTest, AddRemoveObserver) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->RemoveObserver(observer.get());
  webrtc_internals_->AddPeerConnection(3, 4, kUrl, kServers, kContraints);
  EXPECT_EQ("", observer->data());

  webrtc_internals_->RemovePeerConnection(3, 4);
}

TEST_F(WebRTCInternalsTest, SendAddPeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, 2, kUrl, kServers, kContraints);
  EXPECT_EQ("addPeerConnection", observer->data());

  webrtc_internals_->RemoveObserver(observer.get());
  webrtc_internals_->RemovePeerConnection(1, 2);
}

TEST_F(WebRTCInternalsTest, SendRemovePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, 2, kUrl, kServers, kContraints);
  webrtc_internals_->RemovePeerConnection(1, 2);
  EXPECT_EQ("removePeerConnection", observer->data());

  webrtc_internals_->RemoveObserver(observer.get());
}
