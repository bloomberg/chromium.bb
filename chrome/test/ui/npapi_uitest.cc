// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <comutil.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>
#endif

#include <memory.h>
#include <stdlib.h>
#include <string.h>

#include <ostream>

#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/common/content_switches.h"

using npapi_test::kTestCompleteCookie;
using npapi_test::kTestCompleteSuccess;

namespace {

const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("npapi");

class NPAPIAutomationEnabledTest : public NPAPIVisiblePluginTester {
 public:
  NPAPIAutomationEnabledTest() {
    dom_automation_enabled_ = true;
  }
};

}

// Test passing arguments to a plugin.
TEST_F(NPAPITesterBase, Arguments) {
  const FilePath test_case(FILE_PATH_LITERAL("arguments.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("arguments", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Test invoking many plugins within a single page.
TEST_F(NPAPITesterBase, ManyPlugins) {
  const FilePath test_case(FILE_PATH_LITERAL("many_plugins.html"));
  GURL url(ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  for (int i = 1; i <= 15; i++) {
    SCOPED_TRACE(StringPrintf("Waiting for plugin #%d", i));
    ASSERT_NO_FATAL_FAILURE(WaitForFinish(
        "arguments",
        base::IntToString(i),
        url,
        kTestCompleteCookie,
        kTestCompleteSuccess,
        TestTimeouts::action_max_timeout_ms()));
  }
}

// Test various calls to GetURL from a plugin.
TEST_F(NPAPITesterBase, GetURL) {
  const FilePath test_case(FILE_PATH_LITERAL("geturl.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("geturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Test various calls to GetURL for javascript URLs with
// non NULL targets from a plugin.
TEST_F(NPAPITesterBase, GetJavaScriptURL) {
  const FilePath test_case(FILE_PATH_LITERAL("get_javascript_url.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("getjavascripturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Test that calling GetURL with a javascript URL and target=_self
// works properly when the plugin is embedded in a subframe.
TEST_F(NPAPITesterBase, GetJavaScriptURL2) {
  const FilePath test_case(FILE_PATH_LITERAL("get_javascript_url2.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("getjavascripturl2", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Test is flaky on linux/cros/win builders.  http://crbug.com/71904
TEST_F(NPAPITesterBase, FLAKY_GetURLRedirectNotification) {
  const FilePath test_case(FILE_PATH_LITERAL("geturl_redirect_notify.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("geturlredirectnotify", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Tests that identity is preserved for NPObjects passed from a plugin
// into JavaScript.
TEST_F(NPAPITesterBase, NPObjectIdentity) {
  const FilePath test_case(FILE_PATH_LITERAL("npobject_identity.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_identity", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
TEST_F(NPAPITesterBase, NPObjectProxy) {
  const FilePath test_case(FILE_PATH_LITERAL("npobject_proxy.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_proxy", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous paint event works correctly
// http://crbug.com/44960
TEST_F(NPAPIVisiblePluginTester,
       FLAKY_SelfDeletePluginInvokeInSynchronousPaint) {
  show_window_ = true;
  const FilePath test_case(
      FILE_PATH_LITERAL("execute_script_delete_in_paint.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("execute_script_delete_in_paint", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}
#endif

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNewStream) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("self_delete_plugin_stream.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_stream", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

TEST_F(NPAPIVisiblePluginTester, DeletePluginInDeallocate) {
  show_window_ = true;
  const FilePath test_case(
      FILE_PATH_LITERAL("plugin_delete_in_deallocate.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("delete_plugin_in_deallocate_test", "signaller", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

#if defined(OS_WIN)
// Tests if a plugin has a non zero window rect.
TEST_F(NPAPIVisiblePluginTester, VerifyPluginWindowRect) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("verify_plugin_window_rect.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("checkwindowrect", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Tests that creating a new instance of a plugin while another one is handling
// a paint message doesn't cause deadlock.
TEST_F(NPAPIVisiblePluginTester, CreateInstanceInPaint) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("create_instance_in_paint.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("create_instance_in_paint", "2", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Tests that putting up an alert in response to a paint doesn't deadlock.
TEST_F(NPAPIVisiblePluginTester, AlertInWindowMessage) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("alert_in_window_message.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  bool modal_dialog_showing = false;
  ui::MessageBoxFlags::DialogButton available_buttons;
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_NE((ui::MessageBoxFlags::DIALOGBUTTON_OK & available_buttons), 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      ui::MessageBoxFlags::DIALOGBUTTON_OK));

  modal_dialog_showing = false;
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_NE((ui::MessageBoxFlags::DIALOGBUTTON_OK & available_buttons), 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      ui::MessageBoxFlags::DIALOGBUTTON_OK));
}

TEST_F(NPAPIVisiblePluginTester, VerifyNPObjectLifetimeTest) {
  show_window_ = true;
  const FilePath test_case(FILE_PATH_LITERAL("npobject_lifetime_test.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_lifetime_test", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

// Tests that we don't crash or assert if NPP_New fails
TEST_F(NPAPIVisiblePluginTester, NewFails) {
  const FilePath test_case(FILE_PATH_LITERAL("new_fails.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("new_fails", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNPNEvaluate) {
  const FilePath test_case(
      FILE_PATH_LITERAL("execute_script_delete_in_npn_evaluate.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_delete_plugin_in_evaluate", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

TEST_F(NPAPIVisiblePluginTester, SelfDeleteCreatePluginInNPNEvaluate) {
  const FilePath test_case(
      FILE_PATH_LITERAL("npn_plugin_delete_create_in_evaluate.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_delete_create_plugin_in_evaluate", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

#endif

// http://crbug.com/17645
// As of 6 July 2011, this test always fails on OS X and is flaky on
// Windows (perhaps due to timing out).
#if defined(OS_MACOSX)
#define MAYBE_OpenPopupWindowWithPlugin DISABLED_OpenPopupWindowWithPlugin
#else
#define MAYBE_OpenPopupWindowWithPlugin FLAKY_OpenPopupWindowWithPlugin
#endif

TEST_F(NPAPIVisiblePluginTester, MAYBE_OpenPopupWindowWithPlugin) {
  const FilePath test_case(
      FILE_PATH_LITERAL("get_javascript_open_popup_with_plugin.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("plugin_popup_with_plugin_target", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_timeout_ms());
}

// Test checking the privacy mode is off.
TEST_F(NPAPITesterBase, PrivateDisabled) {
  const FilePath test_case(FILE_PATH_LITERAL("private.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NPAPITesterBase, ScheduleTimer) {
  const FilePath test_case(FILE_PATH_LITERAL("schedule_timer.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("schedule_timer", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NPAPITesterBase, PluginThreadAsyncCall) {
  const FilePath test_case(FILE_PATH_LITERAL("plugin_thread_async_call.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("plugin_thread_async_call", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Test checking the privacy mode is on.
TEST_F(NPAPIIncognitoTester, PrivateEnabled) {
  const FilePath test_case(FILE_PATH_LITERAL("private.html"));
  GURL url = ui_test_utils::GetFileUrlWithQuery(
      ui_test_utils::GetTestFilePath(FilePath(kTestDir), test_case), "private");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test a browser hang due to special case of multiple
// plugin instances indulged in sync calls across renderer.
TEST_F(NPAPIVisiblePluginTester, MultipleInstancesSyncCalls) {
  const FilePath test_case(
      FILE_PATH_LITERAL("multiple_instances_sync_calls.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("multiple_instances_sync_calls", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
#endif

TEST_F(NPAPIVisiblePluginTester, GetURLRequestFailWrite) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_fail_write.html"))));

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  WaitForFinish("geturl_fail_write", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

#if defined(OS_WIN)
TEST_F(NPAPITesterBase, EnsureScriptingWorksInDestroy) {
  const FilePath test_case(
      FILE_PATH_LITERAL("ensure_scripting_works_in_destroy.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("ensure_scripting_works_in_destroy", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

// This test uses a Windows Event to signal to the plugin that it should crash
// on NP_Initialize.
TEST_F(NPAPITesterBase, NoHangIfInitCrashes) {
  // Only Windows implements the crash service for now.
#if defined(OS_WIN)
  expected_crashes_ = 1;
#endif

  HANDLE crash_event = CreateEvent(NULL, TRUE, FALSE, L"TestPluginCrashOnInit");
  SetEvent(crash_event);
  const FilePath test_case(FILE_PATH_LITERAL("no_hang_if_init_crashes.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  NavigateToURL(url);
  WaitForFinish("no_hang_if_init_crashes", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
  CloseHandle(crash_event);
}

#endif

TEST_F(NPAPIVisiblePluginTester, PluginReferrerTest) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_referrer_test.html"))));

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  WaitForFinish("plugin_referrer_test", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

#if defined(OS_MACOSX)
TEST_F(NPAPIVisiblePluginTester, PluginConvertPointTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  window->SetBounds(gfx::Rect(50, 50, 400, 400));

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("npapi/convert_point.html"))));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  // TODO(stuartmorgan): When the automation system supports sending clicks,
  // change the test to trigger on mouse-down rather than window focus.
  ASSERT_TRUE(browser->BringToFront());
  WaitForFinish("convert_point", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
#endif

TEST_F(NPAPIVisiblePluginTester, ClickToPlay) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                CONTENT_SETTING_BLOCK));

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL("npapi/click_to_play.html"))));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  ASSERT_TRUE(tab->LoadBlockedPlugins());

  WaitForFinish("setup", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NPAPIAutomationEnabledTest, LoadAllBlockedPlugins) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                CONTENT_SETTING_BLOCK));

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("npapi/load_all_blocked_plugins.html"))));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  ASSERT_TRUE(tab->LoadBlockedPlugins());

  WaitForFinish("setup", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());

  ASSERT_TRUE(tab->ExecuteJavaScript("window.inject()"));

  WaitForFinish("setup", "2", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
