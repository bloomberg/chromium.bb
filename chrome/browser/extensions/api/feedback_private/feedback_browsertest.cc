// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/feedback_private.h"

using extensions::api::feedback_private::FeedbackFlow;

namespace {

void StopMessageLoopCallback() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace

namespace extensions {

class FeedbackTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnableUserMediaScreenCapturing);
  }

 protected:
  bool IsFeedbackAppAvailable() {
    return extensions::EventRouter::Get(browser()->profile())
        ->ExtensionHasEventListener(
            extension_misc::kFeedbackExtensionId,
            extensions::api::feedback_private::OnFeedbackRequested::kEventName);
  }

  void StartFeedbackUI(FeedbackFlow flow,
                       const std::string& extra_diagnostics,
                       bool from_assistant = false,
                       bool include_bluetooth_logs = false) {
    base::Closure callback = base::Bind(&StopMessageLoopCallback);
    extensions::FeedbackPrivateGetStringsFunction::set_test_callback(&callback);
    InvokeFeedbackUI(flow, extra_diagnostics, from_assistant,
                     include_bluetooth_logs);
    content::RunMessageLoop();
    extensions::FeedbackPrivateGetStringsFunction::set_test_callback(NULL);
  }

  void VerifyFeedbackAppLaunch() {
    AppWindow* window =
        PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
    ASSERT_TRUE(window);
    const Extension* feedback_app = window->GetExtension();
    ASSERT_TRUE(feedback_app);
    EXPECT_EQ(feedback_app->id(),
              std::string(extension_misc::kFeedbackExtensionId));
  }

 private:
  void InvokeFeedbackUI(FeedbackFlow flow,
                        const std::string& extra_diagnostics,
                        bool from_assistant,
                        bool include_bluetooth_logs) {
    extensions::FeedbackPrivateAPI* api =
        extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(
            browser()->profile());
    api->RequestFeedbackForFlow("Test description", "Test placeholder",
                                "Test tag", extra_diagnostics,
                                GURL("http://www.test.com"), flow,
                                from_assistant, include_bluetooth_logs);
  }
};

// Disabled for ASan due to flakiness on Mac ASan 64 Tests (1).
// See crbug.com/757243.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ShowFeedback DISABLED_ShowFeedback
#else
#define MAYBE_ShowFeedback ShowFeedback
#endif
IN_PROC_BROWSER_TEST_F(FeedbackTest, MAYBE_ShowFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, std::string());
  VerifyFeedbackAppLaunch();
}

// Disabled for ASan due to flakiness on Mac ASan 64 Tests (1).
// See crbug.com/757243.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ShowLoginFeedback DISABLED_ShowLoginFeedback
#else
#define MAYBE_ShowLoginFeedback ShowLoginFeedback
#endif
IN_PROC_BROWSER_TEST_F(FeedbackTest, MAYBE_ShowLoginFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_LOGIN, std::string());
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
        "$('page-url').hidden && $('attach-file-container').hidden && "
        "$('attach-file-note').hidden);",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

// Disabled for ASan due to flakiness on Mac ASan 64 Tests (1).
// See crbug.com/757243.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AnonymousUser DISABLED_AnonymousUser
#else
#define MAYBE_AnonymousUser AnonymousUser
#endif
// Tests that there's an option in the email drop down box with a value
// 'anonymous_user'.
IN_PROC_BROWSER_TEST_F(FeedbackTest, MAYBE_AnonymousUser) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, std::string());
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var options = $('user-email-drop-down').options;"
      "      for (var option in options) {"
      "        if (options[option].value == 'anonymous_user')"
      "          return true;"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));

  EXPECT_TRUE(bool_result);
}

// Disabled for ASan due to flakiness on Mac ASan 64 Tests (1).
// See crbug.com/757243.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ExtraDiagnostics DISABLED_ExtraDiagnostics
#else
#define MAYBE_ExtraDiagnostics ExtraDiagnostics
#endif
// Ensures that when extra diagnostics are provided with feedback, they are
// injected properly in the system information.
IN_PROC_BROWSER_TEST_F(FeedbackTest, MAYBE_ExtraDiagnostics) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, "Some diagnostics");
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var sysInfo = feedbackInfo.systemInformation;"
      "      for (var info in sysInfo) {"
      "        if (sysInfo[info].key == 'EXTRA_DIAGNOSTICS' &&"
      "            sysInfo[info].value == 'Some diagnostics') {"
      "          return true;"
      "        }"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));

  EXPECT_TRUE(bool_result);
}

// Disabled for ASan due to flakiness on Mac ASan 64 Tests (1).
// See crbug.com/757243.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ShowFeedbackFromAssistant DISABLED_ShowFeedbackFromAssistant
#else
#define MAYBE_ShowFeedbackFromAssistant ShowFeedbackFromAssistant
#endif
// Ensures that when triggered from Assistant with Google account, Assistant
// checkbox are not hidden.
IN_PROC_BROWSER_TEST_F(FeedbackTest, MAYBE_ShowFeedbackFromAssistant) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL, std::string(),
                  /*from_assistant*/ true);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      if ($('assistant-checkbox-container') != null &&"
      "          $('assistant-checkbox-container').hidden == true) {"
      "        return false;"
      "      }"
      "      return true;"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

#if defined(OS_CHROMEOS)
// Ensures that when triggered from a Google account and a Bluetooth related
// string is entered into the description, that we provide the option for
// uploading Bluetooth logs as well.
IN_PROC_BROWSER_TEST_F(FeedbackTest, ProvideBluetoothLogs) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ true);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  // It shouldn't be visible until we put the Bluetooth text into the
  // description.
  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      if ($('bluetooth-checkbox-container') != null &&"
      "          $('bluetooth-checkbox-container').hidden == true) {"
      "        return true;"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
  bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      elem.value = 'bluetooth';"
      "      elem.dispatchEvent(new Event('input', {}));"
      "      if ($('bluetooth-checkbox-container') != null &&"
      "          $('bluetooth-checkbox-container').hidden == false) {"
      "        return true;"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
}
#endif  // if defined(CHROME_OS)
  
IN_PROC_BROWSER_TEST_F(FeedbackTest, GetTargetTabUrl) {
  const std::pair<std::string, std::string> test_cases[] = {
      {"https://www.google.com/", "https://www.google.com/"},
      {"about://version/", chrome::kChromeUIVersionURL},
      {chrome::kChromeUIBookmarksURL, chrome::kChromeUIBookmarksURL},
  };

  for (const auto& test_case : test_cases) {
    GURL expected_url = GURL(test_case.second);

    ui_test_utils::NavigateToURL(browser(), GURL(test_case.first));

    // Sanity check that we always have one tab in the browser.
    ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

    ASSERT_EQ(expected_url,
              browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());

    ASSERT_EQ(expected_url,
              chrome::GetTargetTabUrl(browser()->session_id(), 0));

    // Open a DevTools window.
    DevToolsWindow* devtools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

    // Verify the expected url returned from GetTargetTabUrl against a
    // DevTools window.
    ASSERT_EQ(expected_url, chrome::GetTargetTabUrl(
                                DevToolsWindowTesting::Get(devtools_window)
                                    ->browser()
                                    ->session_id(),
                                0));

    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  }
}
}  // namespace extensions
