// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const string16 kPassTitle = ASCIIToUTF16("Test Passed");
const string16 kFailTitle = ASCIIToUTF16("Test Failed");

// Each test involves calls a single Javascript function and then waits for the
// title to be changed to "Test Passed" or "Test Failed" when done.
class NetInternalsTest : public WebUIBrowserTest {
 public:
  NetInternalsTest();
  virtual ~NetInternalsTest();

  // InProcessBrowserTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  // Runs the specified Javascript test function with the specified arguments
  // and waits for the title to change to "Test Passed" or "Test Failed".
  void RunTestAndWaitForTitle(const std::string& function_name,
                              const ListValue& function_arguments);

  // Same as above, with constant number of arguments for easy use.  Will also
  // free arguments.
  void RunTestAndWaitForTitle(const std::string& function_name);
  void RunTestAndWaitForTitle(const std::string& function_name, Value* arg);
  void RunTestAndWaitForTitle(const std::string& function_name,
                              Value* arg1, Value* arg2);
  void RunTestAndWaitForTitle(const std::string& function_name,
                              Value* arg1, Value* arg2, Value* arg3);

  void set_expected_title_(const string16& value) { expected_title_ = value; }

 private:
  // The expected title at the end of the test.  Defaults to kPassTitle.
  string16 expected_title_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsTest);
};

NetInternalsTest::NetInternalsTest() : expected_title_(kPassTitle) {
}

NetInternalsTest::~NetInternalsTest() {
}

void NetInternalsTest::SetUpCommandLine(CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
  // Needed to test the prerender view.
  command_line->AppendSwitchASCII(switches::kPrerender,
                                  switches::kPrerenderSwitchValueEnabled);
}

void NetInternalsTest::SetUpInProcessBrowserTestFixture() {
  // Adds libraries needed for testing, so much be first.
  WebUIBrowserTest::SetUpInProcessBrowserTestFixture();

  // Framework for net-internals tests.
  AddLibrary(FilePath(FILE_PATH_LITERAL(
                          "net_internals/net_internals_test.js")));

  // Add Javascript files needed for individual tests.
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/log_view_painter.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/prerender_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/test_view.js")));
}

void NetInternalsTest::SetUpOnMainThread() {
  // Navigate to chrome://net-internals.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUINetInternalsURL));
}

void NetInternalsTest::RunTestAndWaitForTitle(
    const std::string& function_name,
    const ListValue& function_arguments) {
  ui_test_utils::TitleWatcher title_watcher(browser()->GetTabContentsAt(0),
                                            kPassTitle);
  title_watcher.AlsoWaitForTitle(kFailTitle);

  ConstValueVector arguments;
  StringValue function_name_arg(function_name);
  arguments.push_back(&function_name_arg);
  arguments.push_back(&function_arguments);

  ASSERT_TRUE(RunJavascriptFunction("netInternalsTest.runTest", arguments));
  ASSERT_EQ(expected_title_, title_watcher.WaitAndGetTitle());
}

void NetInternalsTest::RunTestAndWaitForTitle(
    const std::string& function_name) {
  ListValue test_arguments;
  RunTestAndWaitForTitle(function_name, test_arguments);
}

void NetInternalsTest::RunTestAndWaitForTitle(const std::string& function_name,
                                              Value* arg) {
  ListValue test_arguments;
  test_arguments.Append(arg);
  RunTestAndWaitForTitle(function_name, test_arguments);
}

void NetInternalsTest::RunTestAndWaitForTitle(const std::string& function_name,
                                              Value* arg1, Value* arg2) {
  ListValue test_arguments;
  test_arguments.Append(arg1);
  test_arguments.Append(arg2);
  RunTestAndWaitForTitle(function_name, test_arguments);
}

void NetInternalsTest::RunTestAndWaitForTitle(const std::string& function_name,
                                              Value* arg1, Value* arg2,
                                              Value* arg3) {
  ListValue test_arguments;
  test_arguments.Append(arg1);
  test_arguments.Append(arg2);
  test_arguments.Append(arg3);
  RunTestAndWaitForTitle(function_name, test_arguments);
}

////////////////////////////////////////////////////////////////////////////////
// framework.js
////////////////////////////////////////////////////////////////////////////////

// Checks testDone.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDone) {
  RunTestAndWaitForTitle("NetInternalsDone");
}

// Checks a failed expect statement.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsExpectFail) {
  set_expected_title_(kFailTitle);
  RunTestAndWaitForTitle("NetInternalsExpectFail");
}

// Checks a failed assert statement.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsAssertFail) {
  set_expected_title_(kFailTitle);
  RunTestAndWaitForTitle("NetInternalsAssertFail");
}

// Checks that testDone works when called by an observer in response to an
// event.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsObserverDone) {
  RunTestAndWaitForTitle("NetInternalsObserverDone");
}

// Checks that a failed expect works when called by an observer in response
// to an event.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsObserverExpectFail) {
  set_expected_title_(kFailTitle);
  RunTestAndWaitForTitle("NetInternalsObserverExpectFail");
}

// Checks that a failed assertion works when called by an observer in response
// to an event.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsObserverAssertFail) {
  set_expected_title_(kFailTitle);
  RunTestAndWaitForTitle("NetInternalsObserverAssertFail");
}

////////////////////////////////////////////////////////////////////////////////
// test_view.js
////////////////////////////////////////////////////////////////////////////////

// Runs the test suite twice, expecting a passing result the first time.  Checks
// the first result, the order of events that occur, and the number of rows in
// the table.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTestViewPassTwice) {
  ASSERT_TRUE(test_server()->Start());
  RunTestAndWaitForTitle(
      "NetInternalsTestView",
      // URL that results in success.
      Value::CreateStringValue(
          test_server()->GetURL("files/title1.html").spec()),
      // Resulting error code of the first test.
      Value::CreateIntegerValue(net::OK),
      // Number of times to run the test suite.
      Value::CreateIntegerValue(2));
}

// Runs the test suite twice, expecting a failing result the first time.  Checks
// the first result, the order of events that occur, and the number of rows in
// the table.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTestViewFailTwice) {
  RunTestAndWaitForTitle(
      "NetInternalsTestView",
      // URL that results in an error, due to the port.
      Value::CreateStringValue("http://127.0.0.1:7/"),
      // Resulting error code of the first test.
      Value::CreateIntegerValue(net::ERR_UNSAFE_PORT),
      // Number of times to run the test suite.
      Value::CreateIntegerValue(2));
}

////////////////////////////////////////////////////////////////////////////////
// prerender_view.js
////////////////////////////////////////////////////////////////////////////////

// Prerender two pages and check PrerenderView behavior.  The first is expected
// to fail, the second is expected to succeed.
// Test flaky on XP due to http://crbug.com/91799.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, FLAKY_NetInternalsPrerenderView) {
  ASSERT_TRUE(test_server()->Start());
  RunTestAndWaitForTitle(
      "NetInternalsPrerenderView",
      // URL that can't be prerendered, since it triggers a download.
      Value::CreateStringValue(
          test_server()->GetURL("files/download-test1.lib").spec()),
      // URL that can be prerendered.
      Value::CreateStringValue(
          test_server()->GetURL("files/title1.html").spec()));
}

////////////////////////////////////////////////////////////////////////////////
// log_view_painter.js
////////////////////////////////////////////////////////////////////////////////

// Check that we correctly remove cookies and login information.
IN_PROC_BROWSER_TEST_F(NetInternalsTest,
                       NetInternalsLogViewPainterStripInfo) {
  RunTestAndWaitForTitle("NetInternalsLogViewPainterStripInfo");
}

}  // namespace
