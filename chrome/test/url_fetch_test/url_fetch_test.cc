// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

namespace {

// Provides a UI Test that lets us take the browser to a url, and
// wait for a cookie value to be set before closing the page.
class UrlFetchTest : public UITest {
 public:
  UrlFetchTest() {
    show_window_ = true;
    dom_automation_enabled_ = true;
  }
  struct UrlFetchTestResult {
    std::string cookie_value;
    std::string javascript_variable;
  };

  void SetUp() {
    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch("reference_build")) {
      FilePath dir;
      PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
      dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
      dir = dir.AppendASCII("chrome");
#elif defined(OS_LINUX)
      dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
      dir = dir.AppendASCII("chrome_mac");
#endif
      browser_directory_ = dir;
    }
    UITest::SetUp();
  }

  void RunTest(const GURL& url, const char* wait_cookie_name,
               const char* wait_cookie_value, const char* var_to_fetch,
               UrlFetchTestResult* result) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url));

    if (wait_cookie_name) {
      if (wait_cookie_value) {
        bool completed = WaitUntilCookieValue(tab.get(), url, wait_cookie_name,
                                              UITest::test_timeout_ms(),
                                              wait_cookie_value);
        ASSERT_TRUE(completed);
      } else {
        result->cookie_value = WaitUntilCookieNonEmpty(
            tab.get(), url, wait_cookie_name, UITest::test_timeout_ms());
        ASSERT_TRUE(result->cookie_value.length());
      }
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

bool WriteValueToFile(std::string value, const FilePath& path) {
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

  std::string jsvar = cmd_line->GetSwitchValueASCII("jsvar");

  UrlFetchTestResult result;
  RunTest(GURL(cmd_line->GetSwitchValueASCII("url")),
          cookie_name.length() > 0 ? cookie_name.c_str() : NULL,
          cookie_value.length() > 0 ? cookie_value.c_str() : NULL,
          jsvar.length() > 0 ? jsvar.c_str() : NULL,
          &result);

  // Write out the cookie if requested
  FilePath cookie_output_path =
      cmd_line->GetSwitchValuePath("wait_cookie_output");
  if (cookie_output_path.value().size() > 0) {
    ASSERT_TRUE(WriteValueToFile(result.cookie_value, cookie_output_path));
  }

  // Write out the JS Variable if requested
  FilePath jsvar_output_path = cmd_line->GetSwitchValuePath("jsvar_output");
  if (jsvar_output_path.value().size() > 0) {
    ASSERT_TRUE(WriteValueToFile(result.javascript_variable,
                                 jsvar_output_path));
  }
}

}  // namespace
