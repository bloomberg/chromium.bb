// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/test_server.h"

// Top-level integration test for WebRTC. Requires a real webcam and microphone
// on the running system. This test is not meant to run in the main browser
// test suite since normal tester machines do not have webcams.
class WebrtcBrowserTest : public InProcessBrowserTest {
 public:
  WebrtcBrowserTest() {}

  void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(phoglund): check that user actually has the requisite devices and
    // print a nice message if not; otherwise the test just times out which can
    // be confusing.
    // This test expects real device handling and requires a real webcam / audio
    // device; it will not work with fake devices.
    ASSERT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
  }

  // TODO(phoglund): This ugly poll method is only here while we transition
  // the test javascript to just post events when things happen. Right now they
  // don't because the webrtc_call.py and other tests use this polling way of
  // communicating when we are waiting from an asynchronous event in the
  // javascript. This method is meant to emulate WaitUntil in the PyAuto
  // framework.
  bool UglyPollingWaitUntil(const std::string& javascript,
                            const std::string& evaluates_to) {
    base::Time start_time = base::Time::Now();
    base::TimeDelta timeout = base::TimeDelta::FromSeconds(20);
    std::string result;

    while (base::Time::Now() - start_time < timeout) {
      EXPECT_TRUE(content::ExecuteScriptAndExtractString(
          chrome::GetActiveWebContents(browser()),
          "obtainGetUserMediaResult();",
          &result));
      if (result == evaluates_to)
        return true;
    }
    LOG(ERROR) << "Timed out while waiting for " << javascript <<
        " to evaluate to " << evaluates_to << "; last result was '" << result <<
        "'";
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_InfobarAppearsWhenRequestingUserMedia) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL("files/webrtc/webrtc_jsep01_test.html"));

  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Request user media: this will launch the media stream info bar.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      chrome::GetActiveWebContents(browser()),
      "getUserMedia('{video: true}');",
      &result));
  EXPECT_EQ("ok-requested", result);

  // Wait for the bar to pop up, then accept.
  infobar_added.Wait();
  InfoBarDelegate* infobar =
      content::Details<InfoBarAddedDetails>(infobar_added.details()).ptr();
  MediaStreamInfoBarDelegate* media_infobar =
      infobar->AsMediaStreamInfoBarDelegate();
  media_infobar->Accept();

  // Wait for WebRTC to call the success callback.
  EXPECT_TRUE(UglyPollingWaitUntil("obtainGetUserMediaResult();",
                                   "ok-got-stream"));
}
