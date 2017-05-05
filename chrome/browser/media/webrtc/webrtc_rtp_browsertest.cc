// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"

static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

class WebRtcRtpBrowserTest : public WebRtcTestBase {
 public:
  WebRtcRtpBrowserTest() : left_tab_(nullptr), right_tab_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "RTCRtpSender");
  }

 protected:
  void StartServerAndOpenTabs() {
    ASSERT_TRUE(embedded_test_server()->Start());
    left_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  }

  content::WebContents* left_tab_;
  content::WebContents* right_tab_;
};

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest, GetSenders) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  CreateAndAddStreams(left_tab_, 3);

  SetupPeerconnectionWithoutLocalStream(right_tab_);
  CreateAndAddStreams(right_tab_, 1);

  NegotiateCall(left_tab_, right_tab_);

  VerifyRtpSenders(left_tab_, 6);
  VerifyRtpSenders(right_tab_, 2);
}

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest, GetReceivers) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  CreateAndAddStreams(left_tab_, 3);

  SetupPeerconnectionWithoutLocalStream(right_tab_);
  CreateAndAddStreams(right_tab_, 1);

  NegotiateCall(left_tab_, right_tab_);

  VerifyRtpReceivers(left_tab_, 2);
  VerifyRtpReceivers(right_tab_, 6);
}
