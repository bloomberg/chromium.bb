// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals_message_handler.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_ui.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const char kConstraints[] = "c";
static const char kRtcConfiguration[] = "r";
static const char kUrl[] = "u";

class WebRTCInternalsMessageHandlerForTest
    : public WebRTCInternalsMessageHandler {
 public:
  WebRTCInternalsMessageHandlerForTest(WebRTCInternals* webrtc_internals,
                                       WebUI* web_ui)
      : WebRTCInternalsMessageHandler(webrtc_internals) {
    set_web_ui(web_ui);
  }

  ~WebRTCInternalsMessageHandlerForTest() override {}
};

class WebRTCInternalsForTest : public WebRTCInternals {
 public:
  WebRTCInternalsForTest() : WebRTCInternals(0, false) {}
  ~WebRTCInternalsForTest() override {}
};

}  // namespace

class WebRtcInternalsMessageHandlerTest : public RenderViewHostTestHarness {
 public:
  WebRtcInternalsMessageHandlerTest() {}

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    web_ui_.reset(new TestWebUI());
    web_ui_->set_web_contents(web_contents());
  }

  void TearDown() override {
    web_ui_->set_web_contents(nullptr);
    web_ui_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<TestWebUI> web_ui_;
};

TEST_F(WebRtcInternalsMessageHandlerTest, DontRunJSBeforeNavigationCommitted) {
  GURL webrtc_url(std::string("chrome://") + kChromeUIWebRTCInternalsHost);
  GURL example_url("http://www.example.com/");

  WebRTCInternalsForTest webrtc_internals;
  WebRTCInternalsMessageHandlerForTest observer(&webrtc_internals,
                                                web_ui_.get());

  NavigateAndCommit(example_url);
  webrtc_internals.OnAddPeerConnection(0, 1, 2, kUrl, kRtcConfiguration,
                                       kConstraints);
  base::RunLoop().RunUntilIdle();

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      webrtc_url, web_contents());
  navigation->Start();
  // We still shouldn't run JS, since navigation to webrtc-internals isn't
  // finished.
  webrtc_internals.OnRemovePeerConnection(1, 2);
  base::RunLoop().RunUntilIdle();

  webrtc_internals.RemoveObserver(&observer);
}

}  // namespace content
