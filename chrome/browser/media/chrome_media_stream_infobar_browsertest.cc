// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

#if defined(OS_WIN) && defined(USE_ASH)
#include "base/win/windows_version.h"
#endif

static const char kMainWebrtcTestHtmlPage[] =
    "files/webrtc/webrtc_jsep01_test.html";
static const char kFailedWithErrorPermissionDenied[] =
    "failed-with-error-PERMISSION_DENIED";

static const char kAudioVideoCallConstraints[] = "'{audio: true, video: true}'";
static const char kAudioOnlyCallConstraints[] = "'{audio: true}'";
static const char kVideoOnlyCallConstraints[] = "'{video: true}'";
static const char kOkGotStream[] = "ok-got-stream";

// Media stream infobar test for WebRTC.
class MediaStreamInfobarTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // This test expects to run with fake devices but real UI.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream))
        << "Since this test tests the UI we want the real UI!";
  }
 protected:
  content::WebContents* LoadTestPageInTab() {
    EXPECT_TRUE(test_server()->Start());

    ui_test_utils::NavigateToURL(
        browser(), test_server()->GetURL(kMainWebrtcTestHtmlPage));
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // TODO(phoglund): upstream and reuse in other browser tests.
  MediaStreamInfoBarDelegate* GetUserMediaAndWaitForInfobar(
      content::WebContents* tab_contents,
      const std::string& constraints) {
    content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());

    // Request user media: this will launch the media stream info bar.
    GetUserMedia(constraints, tab_contents);

    // Wait for the bar to pop up, then return it
    infobar_added.Wait();
    content::Details<InfoBarAddedDetails> details(infobar_added.details());
    MediaStreamInfoBarDelegate* media_infobar =
        details.ptr()->AsMediaStreamInfoBarDelegate();
    return media_infobar;
  }

  void CloseInfobarInTab(content::WebContents* tab_contents,
                         MediaStreamInfoBarDelegate* infobar) {
    content::WindowedNotificationObserver infobar_removed(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());

    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(tab_contents);
    infobar_service->RemoveInfoBar(infobar);

    infobar_removed.Wait();
  }

  // Convenience method which executes the provided javascript in the context
  // of the provided web contents and returns what it evaluated to.
  std::string ExecuteJavascript(const std::string& javascript,
                                content::WebContents* tab_contents) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab_contents, javascript, &result));
    return result;
  }

  void TestAcceptOnInfobar(content::WebContents* tab_contents) {
    TestAcceptOnInfobarWithSpecificConstraints(tab_contents,
                                               kAudioVideoCallConstraints);
  }

  void TestAcceptOnInfobarWithSpecificConstraints(
      content::WebContents* tab_contents, const std::string& constraints) {
    MediaStreamInfoBarDelegate* media_infobar =
        GetUserMediaAndWaitForInfobar(tab_contents, constraints);

    media_infobar->Accept();

    CloseInfobarInTab(tab_contents, media_infobar);

    // Wait for WebRTC to call the success callback.
    EXPECT_TRUE(PollingWaitUntil(
        "obtainGetUserMediaResult()", kOkGotStream, tab_contents));
  }

  void TestDenyOnInfobar(content::WebContents* tab_contents) {
    return TestDenyWithSpecificConstraints(tab_contents,
                                           kAudioVideoCallConstraints);
  }

  void TestDenyWithSpecificConstraints(content::WebContents* tab_contents,
                                       const std::string& constraints) {
    MediaStreamInfoBarDelegate* media_infobar =
        GetUserMediaAndWaitForInfobar(tab_contents, constraints);

    media_infobar->Cancel();

    CloseInfobarInTab(tab_contents, media_infobar);

    // Wait for WebRTC to call the fail callback.
    EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                                 kFailedWithErrorPermissionDenied,
                                 tab_contents));
  }

  void TestDismissOnInfobar(content::WebContents* tab_contents) {
    MediaStreamInfoBarDelegate* media_infobar =
        GetUserMediaAndWaitForInfobar(tab_contents, kAudioVideoCallConstraints);

    media_infobar->InfoBarDismissed();

    CloseInfobarInTab(tab_contents, media_infobar);

    // A dismiss should be treated like a deny.
    EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                                 kFailedWithErrorPermissionDenied,
                                 tab_contents));
  }

  void GetUserMedia(const std::string& constraints,
                    content::WebContents* tab_contents) {
    // Request user media: this will launch the media stream info bar.
    EXPECT_EQ("ok-requested",
              ExecuteJavascript(
                  base::StringPrintf("getUserMedia(%s);", constraints.c_str()),
                  tab_contents));
  }
};

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest, TestAllowingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  TestAcceptOnInfobar(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest, TestDenyingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  TestDenyOnInfobar(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest, TestDismissingInfobar) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  TestDismissOnInfobar(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest,
                       TestAcceptThenDenyWhichShouldBeSticky) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return;
#endif

  content::WebContents* tab_contents = LoadTestPageInTab();

  TestAcceptOnInfobar(tab_contents);
  TestDenyOnInfobar(tab_contents);

  // Should fail with permission denied right away with no infobar popping up.
  GetUserMedia(kAudioVideoCallConstraints, tab_contents);
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithErrorPermissionDenied,
                               tab_contents));
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_contents);
  EXPECT_EQ(0u, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest, TestAcceptIsNotSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If accept were sticky the second call would hang because it hangs if an
  // infobar does not pop up.
  TestAcceptOnInfobar(tab_contents);
  TestAcceptOnInfobar(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest, TestDismissIsNotSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If dismiss were sticky the second call would hang because it hangs if an
  // infobar does not pop up.
  TestDismissOnInfobar(tab_contents);
  TestDismissOnInfobar(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest,
                       TestDenyingThenClearingStickyException) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  TestDenyOnInfobar(tab_contents);

  HostContentSettingsMap* settings_map =
      browser()->profile()->GetHostContentSettingsMap();

  settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  // If an infobar is not launched now, this will hang.
  TestDenyOnInfobar(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest,
                       DenyingMicDoesNotCauseStickyDenyForCameras) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If mic blocking also blocked cameras, the second call here would hang.
  TestDenyWithSpecificConstraints(tab_contents, kAudioOnlyCallConstraints);
  TestAcceptOnInfobarWithSpecificConstraints(tab_contents,
                                             kVideoOnlyCallConstraints);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest,
                       DenyingCameraDoesNotCauseStickyDenyForMics) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If camera blocking also blocked mics, the second call here would hang.
  TestDenyWithSpecificConstraints(tab_contents, kVideoOnlyCallConstraints);
  TestAcceptOnInfobarWithSpecificConstraints(tab_contents,
                                             kAudioOnlyCallConstraints);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfobarTest,
                       DenyingMicStillSucceedsWithCameraForAudioVideoCalls) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If microphone blocking also blocked a AV call, the second call here
  // would hang. The requester should only be granted access to the cam though.
  TestDenyWithSpecificConstraints(tab_contents, kAudioOnlyCallConstraints);
  TestAcceptOnInfobarWithSpecificConstraints(tab_contents,
                                             kAudioVideoCallConstraints);

  // TODO(phoglund): verify the requester actually only gets video tracks.
}
