// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

// These tests runs on real webcams and ensure WebRTC can acquire webcams
// correctly. They will do nothing if there are no webcams on the system.
// The webcam on the system must support up to 1080p, or the test will fail.
class WebRtcWebcamBrowserTest : public WebRtcTestBase {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

 protected:
  std::string GetUserMediaAndGetStreamSize(content::WebContents* tab,
                                           const std::string& constraints) {
    GetUserMediaWithSpecificConstraintsAndAccept(tab, constraints);
    StartDetectingVideo(tab, "local-view");
    WaitForVideoToPlay(tab);
    std::string actual_stream_size = GetStreamSize(tab, "local-view");
    CloseLastLocalStream(tab);
    return actual_stream_size;
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcWebcamBrowserTest,
                       TestAcquiringAndReacquiringWebcam) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  if (!HasWebcamAvailableOnSystem(tab)) {
    LOG(INFO) << "No webcam found on bot: skipping...";
    return;
  }

  EXPECT_EQ("640x480",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraintsVGA));
  EXPECT_EQ("320x240",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraintsQVGA));
  EXPECT_EQ("640x360",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraints360p));
  EXPECT_EQ("1280x720",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraints720p));
  EXPECT_EQ("1920x1080",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraints1080p));
}
