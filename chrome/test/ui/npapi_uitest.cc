// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
// windows headers
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <comutil.h>
#endif

// runtime headers
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <ostream>

#include "base/file_util.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "net/base/net_util.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kLongWaitTimeout = 30 * 1000;
const int kShortWaitTimeout = 5 * 1000;

// Test passing arguments to a plugin.
TEST_F(NPAPITester, Arguments) {
  std::wstring test_case = L"arguments.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("arguments", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test invoking many plugins within a single page.
// Flaky, http://crbug.com/28372
TEST_F(NPAPITester, FLAKY_ManyPlugins) {
  std::wstring test_case = L"many_plugins.html";
  GURL url(GetTestUrl(L"npapi", test_case));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  for (int i = 1; i <= 15; i++) {
    SCOPED_TRACE(StringPrintf("Iteration %d", i));
    WaitForFinish("arguments", IntToString(i), url, kTestCompleteCookie,
                  kTestCompleteSuccess, kShortWaitTimeout);
  }
}

// Test various calls to GetURL from a plugin.
TEST_F(NPAPITester, GetURL) {
  std::wstring test_case = L"geturl.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("geturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test various calls to GetURL for javascript URLs with
// non NULL targets from a plugin.
TEST_F(NPAPITester, GetJavaScriptURL) {
  std::wstring test_case = L"get_javascript_url.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("getjavascripturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Flaky test: http://crbug.com/29020
// Test that calling GetURL with a javascript URL and target=_self
// works properly when the plugin is embedded in a subframe.
TEST_F(NPAPITester, FLAKY_GetJavaScriptURL2) {
  std::wstring test_case = L"get_javascript_url2.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("getjavascripturl2", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
TEST_F(NPAPITester, NPObjectProxy) {
  std::wstring test_case = L"npobject_proxy.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_proxy", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
TEST_F(NPAPITester, SelfDeletePluginGetUrl) {
  std::wstring test_case = L"self_delete_plugin_geturl.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_geturl", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
// Flaky. See http://crbug.com/30702
TEST_F(NPAPITester, FLAKY_SelfDeletePluginInvoke) {
  std::wstring test_case = L"self_delete_plugin_invoke.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_invoke", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using Invoke with
// a modal dialog showing works without crashing or hanging
TEST_F(NPAPITester, DISABLED_SelfDeletePluginInvokeAlert) {
  std::wstring test_case = L"self_delete_plugin_invoke_alert.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  // Wait for the alert dialog and then close it.
  automation()->WaitForAppModalDialog(5000);
  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_ESCAPE, 0));

  WaitForFinish("self_delete_plugin_invoke_alert", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous paint event works correctly
TEST_F(NPAPIVisiblePluginTester,
       DISABLED_SelfDeletePluginInvokeInSynchronousPaint) {
  if (UITest::in_process_renderer())
    return;

  show_window_ = true;
  std::wstring test_case = L"execute_script_delete_in_paint.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("execute_script_delete_in_paint", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}
#endif

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNewStream) {
  if (UITest::in_process_renderer())
    return;

  show_window_ = true;
  std::wstring test_case = L"self_delete_plugin_stream.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_stream", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

#if defined(OS_WIN)
// Tests if a plugin has a non zero window rect.
TEST_F(NPAPIVisiblePluginTester, VerifyPluginWindowRect) {
  show_window_ = true;
  std::wstring test_case = L"verify_plugin_window_rect.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("checkwindowrect", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests that creating a new instance of a plugin while another one is handling
// a paint message doesn't cause deadlock.
TEST_F(NPAPIVisiblePluginTester, CreateInstanceInPaint) {
  show_window_ = true;
  std::wstring test_case = L"create_instance_in_paint.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("create_instance_in_paint", "2", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests that putting up an alert in response to a paint doesn't deadlock.
TEST_F(NPAPIVisiblePluginTester, AlertInWindowMessage) {
  show_window_ = true;
  std::wstring test_case = L"alert_in_window_message.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  bool modal_dialog_showing = false;
  MessageBoxFlags::DialogButton available_buttons;
  ASSERT_TRUE(automation()->WaitForAppModalDialog(kShortWaitTimeout));
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_NE((MessageBoxFlags::DIALOGBUTTON_OK & available_buttons), 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));

  modal_dialog_showing = false;
  ASSERT_TRUE(automation()->WaitForAppModalDialog(kShortWaitTimeout));
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_NE((MessageBoxFlags::DIALOGBUTTON_OK & available_buttons), 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
}

TEST_F(NPAPIVisiblePluginTester, VerifyNPObjectLifetimeTest) {
  if (UITest::in_process_renderer())
    return;

  show_window_ = true;
  std::wstring test_case = L"npobject_lifetime_test.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_lifetime_test", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// Tests that we don't crash or assert if NPP_New fails
TEST_F(NPAPIVisiblePluginTester, NewFails) {
  GURL url = GetTestUrl(L"npapi", L"new_fails.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("new_fails", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNPNEvaluate) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi",
                        L"execute_script_delete_in_npn_evaluate.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_delete_plugin_in_evaluate", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}
#endif

// Flaky. See http://crbug.com/17645
TEST_F(NPAPIVisiblePluginTester, DISABLED_OpenPopupWindowWithPlugin) {
  GURL url = GetTestUrl(L"npapi",
                        L"get_javascript_open_popup_with_plugin.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("plugin_popup_with_plugin_target", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_timeout_ms());
}

// Test checking the privacy mode is off.
TEST_F(NPAPITester, PrivateDisabled) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"private.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

TEST_F(NPAPITester, ScheduleTimer) {
  GURL url = GetTestUrl(L"npapi", L"schedule_timer.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("schedule_timer", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

TEST_F(NPAPITester, PluginThreadAsyncCall) {
  GURL url = GetTestUrl(L"npapi", L"plugin_thread_async_call.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("plugin_thread_async_call", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test checking the privacy mode is on.
TEST_F(NPAPIIncognitoTester, PrivateEnabled) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"private.html?private");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test a browser hang due to special case of multiple
// plugin instances indulged in sync calls across renderer.
TEST_F(NPAPIVisiblePluginTester, MultipleInstancesSyncCalls) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"multiple_instances_sync_calls.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("multiple_instances_sync_calls", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}
#endif

TEST_F(NPAPIVisiblePluginTester, GetURLRequestFailWrite) {
  if (UITest::in_process_renderer())
    return;

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_fail_write.html"))));

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  WaitForFinish("geturl_fail_write", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

#if defined(OS_WIN)
TEST_F(NPAPITester, EnsureScriptingWorksInDestroy) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"ensure_scripting_works_in_destroy.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("ensure_scripting_works_in_destroy", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// This test uses a Windows Event to signal to the plugin that it should crash
// on NP_Initialize.
// This is flaky. http://crbug.com/32048
TEST_F(NPAPITester, FLAKY_NoHangIfInitCrashes) {
  if (UITest::in_process_renderer())
    return;

  // Only Windows implements the crash service for now.
#if defined(OS_WIN)
  expected_crashes_ = 1;
#endif

  HANDLE crash_event = CreateEvent(NULL, TRUE, FALSE, L"TestPluginCrashOnInit");
  SetEvent(crash_event);
  GURL url = GetTestUrl(L"npapi", L"no_hang_if_init_crashes.html");
  NavigateToURL(url);
  WaitForFinish("no_hang_if_init_crashes", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
  CloseHandle(crash_event);
}

#endif

TEST_F(NPAPITester, NPObjectReleasedOnDestruction) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"npobject_released_on_destruction.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
  window_proxy->AppendTab(GURL(chrome::kAboutBlankURL));

  scoped_refptr<TabProxy> tab_proxy(window_proxy->GetTab(0));
  tab_proxy->Close(true);
}

// Test that a dialog is properly created when a plugin throws an
// exception.  Should be run for in and out of process plugins, but
// the more interesting case is out of process, where we must route
// the exception to the correct renderer.
TEST_F(NPAPITester, NPObjectSetException) {
  GURL url = GetTestUrl(L"npapi", L"npobject_set_exception.html");
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_set_exception", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

TEST_F(NPAPIVisiblePluginTester, PluginReferrerTest) {
  if (UITest::in_process_renderer())
    return;

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_referrer_test.html"))));

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  WaitForFinish("plugin_referrer_test", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

#if defined(OS_MACOSX)
// http://crbug.com/36670 - failes on 10.6
TEST_F(NPAPIVisiblePluginTester, FLAKY_PluginConvertPointTest) {
  if (UITest::in_process_renderer())
    return;

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  window->SetBounds(gfx::Rect(100, 100, 600, 600));

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("npapi/convert_point.html"))));
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  // TODO(stuartmorgan): When the automation system supports sending clicks,
  // change the test to trigger on mouse-down rather than window focus.
  browser->BringToFront();
  WaitForFinish("convert_point", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}
#endif

