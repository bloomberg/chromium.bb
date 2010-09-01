// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//
// NPAPI interactive UI tests.
//

#include "base/file_path.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "chrome/test/ui_test_utils.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("npapi");

// Tests if a plugin executing a self deleting script in the context of
// a synchronous mousemove works correctly
TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInvokeInSynchronousMouseMove) {
  if (!UITest::in_process_renderer()) {
    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    HWND tab_window = NULL;
    tab_proxy->GetHWND(&tab_window);

    EXPECT_TRUE(IsWindow(tab_window));

    show_window_ = true;
    const FilePath kTestDir(FILE_PATH_LITERAL("npapi"));
    const FilePath test_case(
        FILE_PATH_LITERAL("execute_script_delete_in_mouse_move.html"));
    GURL url = ui_test_utils::GetTestUrl(kTestDir, test_case);
    NavigateToURL(url);

    POINT cursor_position = {130, 130};
    ClientToScreen(tab_window, &cursor_position);

    double screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1;
    double screen_height = ::GetSystemMetrics(SM_CYSCREEN) - 1;
    double location_x =  cursor_position.x * (65535.0f / screen_width);
    double location_y =  cursor_position.y * (65535.0f / screen_height);

    INPUT input_info = {0};
    input_info.type = INPUT_MOUSE;
    input_info.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input_info.mi.dx = static_cast<long>(location_x);
    input_info.mi.dy = static_cast<long>(location_y);
    ::SendInput(1, &input_info, sizeof(INPUT));

    WaitForFinish("execute_script_delete_in_mouse_move", "1", url,
                  kTestCompleteCookie, kTestCompleteSuccess,
                  action_max_timeout_ms());
  }
}

TEST_F(NPAPIVisiblePluginTester, GetURLRequest404Response) {
  if (UITest::in_process_renderer())
    return;

  GURL url(URLRequestMockHTTPJob::GetMockUrl(
               FilePath(FILE_PATH_LITERAL(
                            "npapi/plugin_url_request_404.html"))));

  NavigateToURL(url);

  // Wait for the alert dialog and then close it.
  automation()->WaitForAppModalDialog();
  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_ESCAPE, 0));

  WaitForFinish("geturl_404_response", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
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
  ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_ESCAPE, 0));

  WaitForFinish("self_delete_plugin_invoke_alert", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_max_timeout_ms());
}
