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
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

static const char* kTestConfigFlags[] = {
#if defined(OS_WIN)
  switches::kForceDirectShowVideoCapture,
  // Media Foundation is only available in Windows versions >= 7, below that the
  // following flag has no effect; the test would run twice using DirectShow.
  switches::kForceMediaFoundationVideoCapture
#elif defined(OS_MACOSX)
  switches::kForceQTKit,
  switches::kEnableAVFoundation
#else
  NULL
#endif
};

// These tests runs on real webcams and ensure WebRTC can acquire webcams
// correctly. They will do nothing if there are no webcams on the system.
// The webcam on the system must support up to 1080p, or the test will fail.
class WebRtcWebcamBrowserTest : public WebRtcTestBase,
    public testing::WithParamInterface<const char*> {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));
    if (GetParam())
      command_line->AppendSwitch(GetParam());
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  std::string GetUserMediaAndGetStreamSize(content::WebContents* tab,
                                           const std::string& constraints) {
    GetUserMediaWithSpecificConstraintsAndAccept(tab, constraints);

    StartDetectingVideo(tab, "local-view");
    WaitForVideoToPlay(tab);
    std::string actual_stream_size = GetStreamSize(tab, "local-view");
    CloseLastLocalStream(tab);
    return actual_stream_size;
  }

  bool IsOnQtKit() const {
#if defined(OS_MACOSX)
    return GetParam() && std::string(GetParam()) == switches::kForceQTKit;
#else
    return false;
#endif
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcWebcamBrowserTest,
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

  if (!IsOnQtKit()) {
    // Temporarily disabled on QtKit due to http://crbug.com/375185.
    EXPECT_EQ("320x240",
              GetUserMediaAndGetStreamSize(tab,
                                           kAudioVideoCallConstraintsQVGA));
  }

  EXPECT_EQ("640x480",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraintsVGA));
  EXPECT_EQ("640x360",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraints360p));
  EXPECT_EQ("1280x720",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraints720p));

  if (IsOnQtKit())
    return;  // QTKit only supports up to 720p.

  EXPECT_EQ("1920x1080",
            GetUserMediaAndGetStreamSize(tab, kAudioVideoCallConstraints1080p));
}

INSTANTIATE_TEST_CASE_P(WebRtcWebcamBrowserTests,
                        WebRtcWebcamBrowserTest,
                        testing::ValuesIn(kTestConfigFlags));
