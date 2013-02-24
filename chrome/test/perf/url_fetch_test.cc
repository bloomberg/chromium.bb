// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_perf_test.h"

namespace {

// Provides a UI Test that lets us take the browser to a url, and
// wait for a cookie value to be set or a JavaScript expression to evaluate
// true before closing the page. It is undefined what happens if you specify
// both a cookie and a JS expression.
class UrlFetchTest : public UIPerfTest {
 public:
  UrlFetchTest() {
    show_window_ = true;
    dom_automation_enabled_ = true;
  }
  struct UrlFetchTestResult {
    std::string cookie_value;
    std::string javascript_variable;
  };

  virtual void SetUp() {
    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch("reference_build")) {
      UseReferenceBuild();
    }
    UITest::SetUp();
  }

  void RunTest(const GURL& url,
               const char* wait_cookie_name,
               const char* wait_cookie_value,
               const char* var_to_fetch,
               const std::string& wait_js_expr,
               const std::string& wait_js_frame_xpath,
               base::TimeDelta wait_js_timeout,
               UrlFetchTestResult* result) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url));

    if (wait_cookie_name) {
      if (wait_cookie_value) {
        bool completed = WaitUntilCookieValue(
            tab.get(), url, wait_cookie_name,
            TestTimeouts::large_test_timeout(),
            wait_cookie_value);
        ASSERT_TRUE(completed);
      } else {
        result->cookie_value = WaitUntilCookieNonEmpty(
            tab.get(), url, wait_cookie_name,
            TestTimeouts::large_test_timeout());
        ASSERT_TRUE(result->cookie_value.length());
      }
    } else if (!wait_js_expr.empty()) {
      bool completed = WaitUntilJavaScriptCondition(
          tab.get(),
          UTF8ToWide(wait_js_frame_xpath),
          UTF8ToWide(wait_js_expr),
          wait_js_timeout);
      ASSERT_TRUE(completed);
    }
    if (var_to_fetch) {
      std::string script = StringPrintf(
          "window.domAutomationController.send(%s);", var_to_fetch);

      std::wstring value;
      bool success = tab->ExecuteAndExtractString(L"", ASCIIToWide(script),
                                                  &value);
      ASSERT_TRUE(success);
      result->javascript_variable = WideToUTF8(value);
    }
  }
};

bool WriteValueToFile(std::string value, const base::FilePath& path) {
  int retval = file_util::WriteFile(path, value.c_str(), value.length());
  return retval == static_cast<int>(value.length());
}

// To actually do anything useful, this test should have a url
// passed on the command line, eg.
//
// --url=http://foo.bar.com
//
// Additional arguments:
//
// --wait_cookie_name=<name>
//   Waits for a cookie named <name> to be set before exiting successfully.
//
// --wait_cookie_value=<value>
//   In conjunction with --wait_cookie_name, this waits for a specific value
//   to be set. (Incompatible with --wait_cookie_output)
//
// --wait_cookie_output=<filepath>
//   In conjunction with --wait_cookie_name, this saves the cookie value to
//   a file at the given path. (Incompatible with --wait_cookie_value)
//
// --wait_js_expr=<jscript_expr>
// Waits for a javascript expression to evaluate true before exiting
// successfully.
//
// --wait_js_timeout=<timeout_ms>
// In conjunction with --wait_js_condition, this sets the timeout in ms
// that we are prepared to wait. If this timeout is exceeded, we will exit
// with failure. Note that a timeout greater than the gtest timeout will not
// be honored.
//
// --wait_js_frame_xpath=<xpath>
// In conjuction with --wait_js_condition, the JavaScript expression is
// executed in the context of the frame that matches the provided xpath.
// If this is not specified (or empty string), then the main frame is used.
//
// --jsvar=<name>
//   At the end of the test, fetch the named javascript variable from the page.
//
// --jsvar_output=<filepath>
//   Write the value of the variable named by '--jsvar' to a file at the given
//   path.
//
// --reference_build
//   Use the reference build of chrome for the test.
TEST_F(UrlFetchTest, UrlFetch) {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  if (!cmd_line->HasSwitch("url"))
    return;

  std::string cookie_name =
      cmd_line->GetSwitchValueASCII("wait_cookie_name");
  std::string cookie_value =
      cmd_line->GetSwitchValueASCII("wait_cookie_value");
  std::string js_expr =
      cmd_line->GetSwitchValueASCII("wait_js_expr");
  std::string js_frame_xpath =
      cmd_line->GetSwitchValueASCII("wait_js_frame_xpath");
  std::string js_timeout_ms_str =
      cmd_line->GetSwitchValueASCII("wait_js_timeout");

  std::string jsvar = cmd_line->GetSwitchValueASCII("jsvar");
  int js_timeout_ms = -1; // no timeout, wait forever

  if (!js_timeout_ms_str.empty())
    base::StringToInt(js_timeout_ms_str, &js_timeout_ms);

  UrlFetchTestResult result;
  RunTest(GURL(cmd_line->GetSwitchValueASCII("url")),
          cookie_name.length() > 0 ? cookie_name.c_str() : NULL,
          cookie_value.length() > 0 ? cookie_value.c_str() : NULL,
          jsvar.length() > 0 ? jsvar.c_str() : NULL,
          js_expr,
          js_frame_xpath,
          base::TimeDelta::FromMilliseconds(js_timeout_ms),
          &result);

  // Write out the cookie if requested
  base::FilePath cookie_output_path =
      cmd_line->GetSwitchValuePath("wait_cookie_output");
  if (!cookie_output_path.value().empty()) {
    ASSERT_TRUE(WriteValueToFile(result.cookie_value, cookie_output_path));
  }

  // Write out the JS Variable if requested
  base::FilePath jsvar_output_path =
      cmd_line->GetSwitchValuePath("jsvar_output");
  if (!jsvar_output_path.value().empty()) {
    ASSERT_TRUE(WriteValueToFile(result.javascript_variable,
                                 jsvar_output_path));
  }
}

}  // namespace
