// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/spawned_test_server/spawned_test_server.h"


// MediaStreamInfoBarTest -----------------------------------------------------

class MediaStreamInfoBarTest : public WebRtcTestBase {
 public:
  MediaStreamInfoBarTest() {}
  virtual ~MediaStreamInfoBarTest() {}

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // This test expects to run with fake devices but real UI.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream))
        << "Since this test tests the UI we want the real UI!";
  }

 protected:
  content::WebContents* LoadTestPageInTab() {
    return LoadTestPageInBrowser(browser());
  }

  content::WebContents* LoadTestPageInIncognitoTab() {
    return LoadTestPageInBrowser(CreateIncognitoBrowser());
  }

  // Returns the URL of the main test page.
  GURL test_page_url() const {
    const char kMainWebrtcTestHtmlPage[] =
        "files/webrtc/webrtc_jsep01_test.html";
    return test_server()->GetURL(kMainWebrtcTestHtmlPage);
  }

  // Denies getUserMedia requests (audio, video) for the test page.
  // The deny setting is sticky.
  void DenyRequest(content::WebContents* tab_contents,
                   content::MediaStreamRequestResult result) const {
    const std::string no_id;
    content::MediaStreamRequest request(
        0, 0, 0, test_page_url().GetOrigin(), false,
        content::MEDIA_DEVICE_ACCESS, no_id, no_id,
        content::MEDIA_DEVICE_AUDIO_CAPTURE,
        content::MEDIA_DEVICE_VIDEO_CAPTURE);

    scoped_ptr<MediaStreamDevicesController> controller(
        new MediaStreamDevicesController(tab_contents, request,
            base::Bind(&OnMediaStreamResponse)));
    controller->Deny(true, result);
  }

  // Executes stopLocalStream() in the test page, which frees up an already
  // acquired mediastream.
  bool StopLocalStream(content::WebContents* tab_contents) {
    std::string result;
    bool ok = content::ExecuteScriptAndExtractString(
        tab_contents, "stopLocalStream()", &result);
    DCHECK(ok);
    return result.compare("ok-stopped") == 0;
  }

 private:
  content::WebContents* LoadTestPageInBrowser(Browser* browser) {
    EXPECT_TRUE(test_server()->Start());

    ui_test_utils::NavigateToURL(browser, test_page_url());
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  // Dummy callback for when we deny the current request directly.
  static void OnMediaStreamResponse(const content::MediaStreamDevices& devices,
                                    content::MediaStreamRequestResult result,
                                    scoped_ptr<content::MediaStreamUI> ui) {}

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarTest);
};

// Actual tests ---------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest, TestAllowingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndAccept(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest, TestDenyingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest, TestDismissingInfobar) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndDismiss(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest, TestDenyingUserMediaIncognito) {
  content::WebContents* tab_contents = LoadTestPageInIncognitoTab();
  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest,
                       TestAcceptThenDenyWhichShouldBeSticky) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaAndAccept(tab_contents);
  GetUserMediaAndDeny(tab_contents);

  // Should fail with permission denied right away with no infobar popping up.
  GetUserMedia(tab_contents, kAudioVideoCallConstraints);
  EXPECT_TRUE(test::PollingWaitUntil("obtainGetUserMediaResult()",
                                     kFailedWithPermissionDeniedError,
                                     tab_contents));
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_contents);
  EXPECT_EQ(0u, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest, TestAcceptIsNotSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If accept were sticky the second call would hang because it hangs if an
  // infobar does not pop up.
  GetUserMediaAndAccept(tab_contents);
  GetUserMediaAndAccept(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest, TestDismissIsNotSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If dismiss were sticky the second call would hang because it hangs if an
  // infobar does not pop up.
  GetUserMediaAndDismiss(tab_contents);
  GetUserMediaAndDismiss(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest,
                       TestDenyingThenClearingStickyException) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaAndDeny(tab_contents);

  HostContentSettingsMap* settings_map =
      browser()->profile()->GetHostContentSettingsMap();

  settings_map->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  // If an infobar is not launched now, this will hang.
  GetUserMediaAndDeny(tab_contents);
}

// Times out on win debug builds; http://crbug.com/295723 .
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_DenyingMicDoesNotCauseStickyDenyForCameras \
        DISABLED_DenyingMicDoesNotCauseStickyDenyForCameras
#else
#define MAYBE_DenyingMicDoesNotCauseStickyDenyForCameras \
        DenyingMicDoesNotCauseStickyDenyForCameras
#endif
IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest,
                       MAYBE_DenyingMicDoesNotCauseStickyDenyForCameras) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If mic blocking also blocked cameras, the second call here would hang.
  GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                             kAudioOnlyCallConstraints);
  GetUserMediaWithSpecificConstraintsAndAccept(tab_contents,
                                               kVideoOnlyCallConstraints);
}

IN_PROC_BROWSER_TEST_F(MediaStreamInfoBarTest,
                       DenyingCameraDoesNotCauseStickyDenyForMics) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  // If camera blocking also blocked mics, the second call here would hang.
  GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                             kVideoOnlyCallConstraints);
  GetUserMediaWithSpecificConstraintsAndAccept(tab_contents,
                                               kAudioOnlyCallConstraints);
}
