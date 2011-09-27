// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Class to handle messages from the renderer needed by certain tests.
class NetInternalsTestMessageHandler : public WebUIMessageHandler {
 public:
  NetInternalsTestMessageHandler();

  void set_browser(Browser* browser) {
    ASSERT_TRUE(browser);
    browser_ = browser;
  }

 private:
  virtual void RegisterMessages() OVERRIDE;

  // Opens the given URL in a new tab.
  void OpenNewTab(const ListValue* list_value);

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsTestMessageHandler);
};

NetInternalsTestMessageHandler::NetInternalsTestMessageHandler()
    : browser_(NULL) {
}

void NetInternalsTestMessageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("openNewTab", NewCallback(
      this, &NetInternalsTestMessageHandler::OpenNewTab));
}

void NetInternalsTestMessageHandler::OpenNewTab(const ListValue* list_value) {
  std::string url;
  ASSERT_TRUE(list_value->GetString(0, &url));
  ASSERT_TRUE(browser_);
  ui_test_utils::NavigateToURLWithDisposition(
      browser_,
      GURL(url),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
}

class NetInternalsTest : public WebUIBrowserTest {
 public:
  NetInternalsTest();
  virtual ~NetInternalsTest();

  // InProcessBrowserTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

 private:
  virtual WebUIMessageHandler* GetMockMessageHandler() OVERRIDE {
    message_handler_.set_browser(browser());
    return &message_handler_;
  }

  NetInternalsTestMessageHandler message_handler_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsTest);
};

NetInternalsTest::NetInternalsTest() {
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
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/hsts_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/log_util.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/log_view_painter.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/main.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/prerender_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/test_view.js")));
}

void NetInternalsTest::SetUpOnMainThread() {
  // Navigate to chrome://net-internals.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUINetInternalsURL));
  // Increase the memory allowed in a prerendered page above normal settings,
  // as debug builds use more memory and often go over the usual limit.
  Profile* profile = browser()->GetSelectedTabContentsWrapper()->profile();
  prerender::PrerenderManager* prerender_manager =
      profile->GetPrerenderManager();
  prerender_manager->mutable_config().max_bytes = 1000 * 1024 * 1024;
}

////////////////////////////////////////////////////////////////////////////////
// net_internals_ui.js
////////////////////////////////////////////////////////////////////////////////

// Checks testDone.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDone) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsDone"));
}

// Checks a failed expect statement.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsExpectFail) {
  EXPECT_FALSE(RunJavascriptAsyncTest("netInternalsExpectFail"));
}

// Checks a failed assert statement.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsAssertFail) {
  EXPECT_FALSE(RunJavascriptAsyncTest("netInternalsAssertFail"));
}

// Checks that testDone works when called by an observer in response to an
// event.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsObserverDone) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsObserverDone"));
}

// Checks that a failed expect works when called by an observer in response
// to an event.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsObserverExpectFail) {
  EXPECT_FALSE(RunJavascriptAsyncTest("netInternalsObserverExpectFail"));
}

// Checks that a failed assertion works when called by an observer in response
// to an event.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsObserverAssertFail) {
  EXPECT_FALSE(RunJavascriptAsyncTest("netInternalsObserverAssertFail"));
}

////////////////////////////////////////////////////////////////////////////////
// main.js
////////////////////////////////////////////////////////////////////////////////

// Checks tabs initialization and switching between tabs.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTourTabs) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTourTabs"));
}

////////////////////////////////////////////////////////////////////////////////
// log_dump_util.js
////////////////////////////////////////////////////////////////////////////////

// Checks exporting and importing a log dump, as well as some tab behavior in
// response to doing this.  Does not actually save the log to a file, just
// to a string.
// TODO(mmenke):  Add some checks for the import view.
// TODO(mmenke):  Add a test for a log created with --log-net-log.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsExportImportDump) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsExportImportDump"));
}

////////////////////////////////////////////////////////////////////////////////
// test_view.js
////////////////////////////////////////////////////////////////////////////////

// Runs the test suite twice, expecting a passing result the first time.  Checks
// the first result, the order of events that occur, and the number of rows in
// the table.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTestViewPassTwice) {
  ASSERT_TRUE(test_server()->Start());
  EXPECT_TRUE(RunJavascriptAsyncTest(
      "netInternalsTestView",
      // URL that results in success.
      Value::CreateStringValue(
          test_server()->GetURL("files/title1.html").spec()),
      // Resulting error code of the first test.
      Value::CreateIntegerValue(net::OK),
      // Number of times to run the test suite.
      Value::CreateIntegerValue(2)));
}

// Runs the test suite twice.  Checks the exact error code of the first result,
// the order of events that occur, and the number of rows in the HTML table.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTestViewFailTwice) {
  EXPECT_TRUE(RunJavascriptAsyncTest(
      "netInternalsTestView",
      // URL that results in an error, due to the port.
      Value::CreateStringValue("http://127.0.0.1:7/"),
      // Resulting error code of the first test.
      Value::CreateIntegerValue(net::ERR_UNSAFE_PORT),
      // Number of times to run the test suite.
      Value::CreateIntegerValue(2)));
}

////////////////////////////////////////////////////////////////////////////////
// hsts_view.js
////////////////////////////////////////////////////////////////////////////////

// Checks that querying a domain that was never added fails.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewQueryNotFound) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewQueryNotFound"));
}

// Checks that querying a domain with an invalid name returns an error.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewQueryError) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewQueryError"));
}

// Deletes a domain that was never added.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewDeleteNotFound) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewDeleteNotFound"));
}

// Deletes a domain that returns an error on lookup.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewDeleteError) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewDeleteNotFound"));
}

// Adds a domain and then deletes it.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewAddDelete) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewAddDelete"));
}

// Tries to add a domain with an invalid name.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewAddFail) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewAddError"));
}

// Tries to add a domain with a name that errors out on lookup due to having
// non-ASCII characters in it.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewAddError) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewAddError"));
}

// Adds a domain with an invalid hash.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewAddInvalidHash) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewAddInvalidHash"));
}

// Adds the same domain twice in a row, modifying some values the second time.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewAddOverwrite) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewAddOverwrite"));
}

// Adds two different domains and then deletes them.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsHSTSViewAddTwice) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsHSTSViewAddTwice"));
}

////////////////////////////////////////////////////////////////////////////////
// prerender_view.js
////////////////////////////////////////////////////////////////////////////////

// Prerender a page and navigate to it, once prerendering starts.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsPrerenderViewSucceed) {
  ASSERT_TRUE(test_server()->Start());
  EXPECT_TRUE(RunJavascriptAsyncTest(
      "netInternalsPrerenderView",
      // URL that can be prerendered.
      Value::CreateStringValue(
          test_server()->GetURL("files/title1.html").spec()),
      Value::CreateBooleanValue(true),
      Value::CreateStringValue(
          prerender::NameFromFinalStatus(prerender::FINAL_STATUS_USED))));
}

// Prerender a page that is expected to fail.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsPrerenderViewFail) {
  ASSERT_TRUE(test_server()->Start());
  EXPECT_TRUE(RunJavascriptAsyncTest(
      "netInternalsPrerenderView",
      // URL that can't be prerendered, since it triggers a download.
      Value::CreateStringValue(
          test_server()->GetURL("files/download-test1.lib").spec()),
      Value::CreateBooleanValue(false),
      Value::CreateStringValue(
          prerender::NameFromFinalStatus(prerender::FINAL_STATUS_DOWNLOAD))));
}

////////////////////////////////////////////////////////////////////////////////
// log_view_painter.js
////////////////////////////////////////////////////////////////////////////////

// Check that we correctly remove cookies and login information.
IN_PROC_BROWSER_TEST_F(NetInternalsTest,
                       NetInternalsLogViewPainterStripInfo) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsLogViewPainterStripInfo"));
}

}  // namespace
