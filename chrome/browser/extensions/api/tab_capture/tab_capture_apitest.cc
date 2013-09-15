// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/complex_feature.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/features/feature.h"

namespace chrome {

namespace {

const char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  TabCaptureApiTest() {}

  virtual void SetUp() OVERRIDE {
    // TODO(danakj): The GPU Video Decoder needs real GL bindings.
    // crbug.com/269087
    UseRealGLBindings();

    ExtensionApiTest::SetUp();
  }

  void AddExtensionToCommandLineWhitelist() {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, kExtensionId);
  }
};

}  // namespace

// http://crbug.com/261493 and http://crbug.com/268644
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_AURA)
#define MAYBE_ApiTests DISABLED_ApiTests
#else
#define MAYBE_ApiTests ApiTests
#endif
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_ApiTests) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

#if defined(OS_WIN)
  // TODO(justinlin): Disabled for WinXP due to timeout issues.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    return;
  }
#endif

  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "api_tests.html")) << message_;
}

// http://crbug.com/268644
#if defined(USE_AURA)
#define MAYBE_ApiTestsAudio DISABLED_ApiTestsAudio
#else
#define MAYBE_ApiTestsAudio ApiTestsAudio
#endif
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_ApiTestsAudio) {
#if defined(OS_WIN)
  // TODO(justinlin): Disabled for WinXP due to timeout issues.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    return;
  }
#endif

  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "api_tests_audio.html")) << message_;
}

// http://crbug.com/177163 and http://crbug.com/268644
#if defined(OS_WIN) && (!defined(NDEBUG) || defined(USE_AURA))
#define MAYBE_EndToEnd DISABLED_EndToEnd
#else
#define MAYBE_EndToEnd EndToEnd
#endif
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_EndToEnd) {
#if defined(OS_WIN)
  // TODO(justinlin): Disabled for WinXP due to timeout issues.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    return;
  }
#endif
#if defined(OS_MACOSX)
  // TODO(miu): Disabled for Mac OS X 10.6 due to timeout issues.
  // http://crbug.com/174640
  if (base::mac::IsOSSnowLeopard())
    return;
#endif

  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "end_to_end.html")) << message_;
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_GetUserMediaTest DISABLED_GetUserMediaTest
#else
#define MAYBE_GetUserMediaTest GetUserMediaTest
#endif
// Test that we can't get tabCapture streams using GetUserMedia directly.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_GetUserMediaTest) {
  ExtensionTestMessageListener listener("ready", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "get_user_media_test.html")) << message_;

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);

  content::RenderViewHost* const rvh = web_contents->GetRenderViewHost();
  int render_process_id = rvh->GetProcess()->GetID();
  int routing_id = rvh->GetRoutingID();

  listener.Reply(base::StringPrintf("%i:%i", render_process_id, routing_id));

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_ActiveTabPermission DISABLED_ActiveTabPermission
#else
#define MAYBE_ActiveTabPermission ActiveTabPermission
#endif
// Make sure tabCapture.capture only works if the tab has been granted
// permission via an extension icon click or the extension is whitelisted.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_ActiveTabPermission) {
  ExtensionTestMessageListener before_open_tab("ready1", true);
  ExtensionTestMessageListener before_grant_permission("ready2", true);
  ExtensionTestMessageListener before_open_new_tab("ready3", true);
  ExtensionTestMessageListener before_whitelist_extension("ready4", true);

  ASSERT_TRUE(RunExtensionSubtest(
      "tab_capture/experimental", "active_tab_permission_test.html"))
          << message_;

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());
  content::OpenURLParams params(GURL("http://google.com"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  before_open_tab.Reply("");

  // Grant permission and make sure capture succeeds.
  EXPECT_TRUE(before_grant_permission.WaitUntilSatisfied());
  ExtensionService* extension_service =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetExtensionService();
  const extensions::Extension* extension =
      extension_service->GetExtensionById(kExtensionId, false);
  extensions::TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()->GrantIfRequested(extension);
  before_grant_permission.Reply("");

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_new_tab.WaitUntilSatisfied());
  browser()->OpenURL(params);
  before_open_new_tab.Reply("");

  // Add extension to whitelist and make sure capture succeeds.
  EXPECT_TRUE(before_whitelist_extension.WaitUntilSatisfied());
  AddExtensionToCommandLineWhitelist();
  before_whitelist_extension.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_FullscreenEvents DISABLED_FullscreenEvents
#elif defined(USE_AURA) || defined(OS_MACOSX)
// These don't always fire fullscreen events when run in tests. Tested manually.
#define MAYBE_FullscreenEvents DISABLED_FullscreenEvents
#elif defined(OS_LINUX)
// Flaky to get out of fullscreen in tests. Tested manually.
#define MAYBE_FullscreenEvents DISABLED_FullscreenEvents
#else
#define MAYBE_FullscreenEvents FullscreenEvents
#endif
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_FullscreenEvents) {
#if defined(OS_WIN)
  // TODO(justinlin): Disabled for WinXP due to timeout issues.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    return;
  }
#endif

  AddExtensionToCommandLineWhitelist();

  content::OpenURLParams params(GURL("chrome://version"),
                                content::Referrer(),
                                CURRENT_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);

  ExtensionTestMessageListener listeners_setup("ready1", true);
  ExtensionTestMessageListener fullscreen_entered("ready2", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "fullscreen_test.html")) << message_;
  EXPECT_TRUE(listeners_setup.WaitUntilSatisfied());

  // Toggle fullscreen after setting up listeners.
  browser()->fullscreen_controller()->ToggleFullscreenModeForTab(web_contents,
                                                                 true);
  listeners_setup.Reply("");

  // Toggle again after JS should have the event.
  EXPECT_TRUE(fullscreen_entered.WaitUntilSatisfied());
  browser()->fullscreen_controller()->ToggleFullscreenModeForTab(web_contents,
                                                                 false);
  fullscreen_entered.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Make sure tabCapture API can be granted for Chrome:// pages.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, GrantForChromePages) {
  ExtensionTestMessageListener before_open_tab("ready1", true);
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "active_tab_chrome_pages.html")) << message_;
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());

  // Open a tab on a chrome:// page and make sure we can capture.
  content::OpenURLParams params(GURL("chrome://version"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  ExtensionService* extension_service =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetExtensionService();
  extensions::TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()->GrantIfRequested(
            extension_service->GetExtensionById(kExtensionId, false));
  before_open_tab.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace chrome
