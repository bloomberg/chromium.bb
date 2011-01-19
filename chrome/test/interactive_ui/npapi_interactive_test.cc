// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NPAPI interactive UI tests.

#include "base/file_path.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "chrome/test/ui_test_utils.h"
#include "ui/base/keycodes/keyboard_codes.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("npapi");

// Tests if a plugin executing a self deleting script in the context of
// a synchronous mousemove works correctly
TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInvokeInSynchronousMouseMove) {
  if (ProxyLauncher::in_process_renderer())
    return;

  show_window_ = true;
  const FilePath kTestDir(FILE_PATH_LITERAL("npapi"));
  const FilePath test_case(
      FILE_PATH_LITERAL("execute_script_delete_in_mouse_move.html"));
  GURL url = ui_test_utils::GetTestUrl(kTestDir, test_case);
  NavigateToURL(url);

  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());

  gfx::Point cursor_position(150, 250);
  window->SimulateOSMouseMove(cursor_position);

  WaitForFinish("execute_script_delete_in_mouse_move", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

// Flaky, http://crbug.com/60071.
TEST_F(NPAPIVisiblePluginTester, FLAKY_GetURLRequest404Response) {
  if (ProxyLauncher::in_process_renderer())
    return;

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_404.html"))));

  NavigateToURL(url);

  // Wait for the alert dialog and then close it.
  automation()->WaitForAppModalDialog();
  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SimulateOSKeyPress(ui::VKEY_ESCAPE, 0));

  WaitForFinish("geturl_404_response", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

// Tests if a plugin executing a self deleting script using Invoke with
// a modal dialog showing works without crashing or hanging
// Disabled, flakily exceeds timeout, http://crbug.com/46257.
TEST_F(NPAPIVisiblePluginTester, DISABLED_SelfDeletePluginInvokeAlert) {
  const FilePath test_case(
      FILE_PATH_LITERAL("self_delete_plugin_invoke_alert.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  // Wait for the alert dialog and then close it.
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SimulateOSKeyPress(ui::VKEY_ESCAPE, 0));

  WaitForFinish("self_delete_plugin_invoke_alert", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}
