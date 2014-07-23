// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

// Top-level integration test for WebRTC. It always uses fake devices; see
// WebRtcWebcamBrowserTest for a test that acquires any real webcam on the
// system.
class WebRtcBrowserTest : public WebRtcTestBase {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Always use fake devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Flag used by TestWebAudioMediaStream to force garbage collection.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }
};

#if defined(OS_WIN) || defined(OS_CHROMEOS)
// http://crbug.com/396505
#define MAYBE_RunsAudioVideoWebRTCCallInTwoTabs \
    DISABLED_RunsAudioVideoWebRTCCallInTwoTabs
#else
#define MAYBE_RunsAudioVideoWebRTCCallInTwoTabs RunsAudioVideoWebRTCCallInTwoTabs
#endif
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       MAYBE_RunsAudioVideoWebRTCCallInTwoTabs) {
  if (OnWinXp()) return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  content::WebContents* left_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  content::WebContents* right_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  SetupPeerconnectionWithLocalStream(left_tab);
  SetupPeerconnectionWithLocalStream(right_tab);

  NegotiateCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  HangUp(left_tab);
  HangUp(right_tab);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, TestWebAudioMediaStream) {
  // This tests against crash regressions for the WebAudio-MediaStream
  // integration.
  if (OnWinXp()) return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL("/webrtc/webaudio_crash.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // A sleep is necessary to be able to detect the crash.
  test::SleepInJavascript(tab, 1000);

  ASSERT_FALSE(tab->IsCrashed());
}
