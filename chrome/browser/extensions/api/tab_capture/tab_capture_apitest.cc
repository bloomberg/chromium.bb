// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/complex_feature.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"

namespace chrome {

namespace {

const char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  TabCaptureApiTest() {}

  void AddExtensionToCommandLineWhitelist() {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, kExtensionId);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ApiTests) {
  extensions::FeatureSwitch::ScopedOverride tab_capture(
      extensions::FeatureSwitch::tab_capture(), true);

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

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ApiTestsAudio) {
  extensions::FeatureSwitch::ScopedOverride tab_capture(
      extensions::FeatureSwitch::tab_capture(), true);

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

// TODO(miu): Disabled until the two most-likely sources of the "flaky timeouts"
// are resolved: 1) http://crbug.com/177163 and 2) http://crbug.com/174519.
// See http://crbug.com/174640.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, DISABLED_EndToEnd) {
  extensions::FeatureSwitch::ScopedOverride tab_capture(
      extensions::FeatureSwitch::tab_capture(), true);
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "end_to_end.html")) << message_;
}

// Test that we can't get tabCapture streams using GetUserMedia directly.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, GetUserMediaTest) {
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

// Make sure tabCapture.capture only works if the tab has been granted
// permission via an extension icon click or the extension is whitelisted.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ActiveTabPermission) {
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

}  // namespace chrome
