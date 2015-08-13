// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

// MediaStreamPermissionTest ---------------------------------------------------

class MediaStreamPermissionTest : public WebRtcTestBase {
 public:
  MediaStreamPermissionTest() {}
  ~MediaStreamPermissionTest() override {}

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
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

  // Executes stopLocalStream() in the test page, which frees up an already
  // acquired mediastream.
  bool StopLocalStream(content::WebContents* tab_contents) {
    std::string result;
    bool ok = content::ExecuteScriptAndExtractString(
        tab_contents, "stopLocalStream()", &result);
    DCHECK(ok);
    return result.compare("ok-stopped") == 0;
  }

  void useNonSecureOriginForTestPage() {
    use_secure_origin_for_test_page_ = false;
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  // The default test server is localhost, which is considered secure:
  // http://www.w3.org/TR/powerful-features/#is-origin-trustworthy
  bool use_secure_origin_for_test_page_ = true;

  content::WebContents* LoadTestPageInBrowser(Browser* browser) {
    EXPECT_TRUE(test_server()->Start());

    GURL url;

    if (use_secure_origin_for_test_page_) {
      // Uses the default server.
      url = test_page_url();
    } else {
      static const char kFoo[] = "not-secure.example.com";
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpScheme);
      replacements.SetHostStr(kFoo);
      url = test_page_url().ReplaceComponents(replacements);
    }

    EXPECT_EQ(use_secure_origin_for_test_page_, content::IsOriginSecure(url));

    ui_test_utils::NavigateToURL(browser, url);
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  // Dummy callback for when we deny the current request directly.
  static void OnMediaStreamResponse(const content::MediaStreamDevices& devices,
                                    content::MediaStreamRequestResult result,
                                    scoped_ptr<content::MediaStreamUI> ui) {}

  DISALLOW_COPY_AND_ASSIGN(MediaStreamPermissionTest);
};

// Actual tests ---------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestAllowingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestDenyingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestDismissingRequest) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndDismiss(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestDenyingUserMediaIncognito) {
  content::WebContents* tab_contents = LoadTestPageInIncognitoTab();
  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestNonSecureOriginAcceptThenDenyIsSticky) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  useNonSecureOriginForTestPage();
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_FALSE(content::IsOriginSecure(tab_contents->GetLastCommittedURL()));

  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
  GetUserMediaAndDeny(tab_contents);

  GetUserMediaAndExpectAutoDenyWithoutPrompt(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestNonSecureOriginDenyIsSticky) {
  useNonSecureOriginForTestPage();
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_FALSE(content::IsOriginSecure(tab_contents->GetLastCommittedURL()));

  GetUserMediaAndDeny(tab_contents);
  GetUserMediaAndExpectAutoDenyWithoutPrompt(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestSecureOriginDenyIsSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_TRUE(content::IsOriginSecure(tab_contents->GetLastCommittedURL()));

  GetUserMediaAndDeny(tab_contents);
  GetUserMediaAndExpectAutoDenyWithoutPrompt(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestNonSecureOriginAcceptIsNotSticky) {
  useNonSecureOriginForTestPage();
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_FALSE(content::IsOriginSecure(tab_contents->GetLastCommittedURL()));

  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestSecureOriginAcceptIsSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_TRUE(content::IsOriginSecure(tab_contents->GetLastCommittedURL()));

  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
  GetUserMediaAndExpectAutoAcceptWithoutPrompt(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestDismissIsNotSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaAndDismiss(tab_contents);
  GetUserMediaAndDismiss(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestDenyingThenClearingStickyException) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaAndDeny(tab_contents);
  GetUserMediaAndExpectAutoDenyWithoutPrompt(tab_contents);

  HostContentSettingsMap* settings_map =
      browser()->profile()->GetHostContentSettingsMap();

  settings_map->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       DenyingMicDoesNotCauseStickyDenyForCameras) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                             kAudioOnlyCallConstraints);
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAccept(
      tab_contents, kVideoOnlyCallConstraints));
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       DenyingCameraDoesNotCauseStickyDenyForMics) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                             kVideoOnlyCallConstraints);
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAccept(
      tab_contents, kAudioOnlyCallConstraints));
}
