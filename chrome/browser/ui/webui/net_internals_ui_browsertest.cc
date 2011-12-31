// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::WebUIMessageHandler;

namespace {

// Called on IO thread.  Adds an entry to the cache for the specified hostname.
// Either |net_error| must be net::OK, or |address| must be NULL.
void AddCacheEntryOnIOThread(net::URLRequestContextGetter* context_getter,
                             const std::string& hostname,
                             const std::string& ip_literal,
                             int net_error,
                             int expire_days_from_now) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::HostCache* cache = context->host_resolver()->GetHostCache();
  ASSERT_TRUE(cache);

  net::HostCache::Key key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0);
  base::TimeTicks expires =
      base::TimeTicks::Now() + base::TimeDelta::FromDays(expire_days_from_now);

  net::AddressList address_list;
  if (net_error == net::OK) {
    // If |net_error| does not indicate an error, convert |ip_literal| to a
    // net::AddressList, so it can be used with the cache.
    int rv = net::SystemHostResolverProc(ip_literal,
                                         net::ADDRESS_FAMILY_UNSPECIFIED,
                                         0,
                                         &address_list,
                                         NULL);
    ASSERT_EQ(net::OK, rv);
  } else {
    ASSERT_TRUE(ip_literal.empty());
  }

  // Add entry to the cache.
  cache->Set(net::HostCache::Key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0),
             net_error,
             address_list,
             expires);
}

class NetInternalsTest : public WebUIBrowserTest {
 public:
  NetInternalsTest();
  virtual ~NetInternalsTest();

  // InProcessBrowserTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

 protected:
  GURL CreatePrerenderLoaderUrl(const GURL& prerender_url) {
    std::vector<net::TestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_PRERENDER_URL", prerender_url.spec()));
    std::string replacement_path;
    EXPECT_TRUE(net::TestServer::GetFilePathWithReplacements(
        "files/prerender/prerender_loader.html",
        replacement_text,
        &replacement_path));
    GURL url_loader = test_server()->GetURL(replacement_path);
    return url_loader;
  }

 private:
  // Class to handle messages from the renderer needed by certain tests.
  class MessageHandler : public WebUIMessageHandler {
   public:
    explicit MessageHandler(NetInternalsTest* net_internals_test);

   private:
    virtual void RegisterMessages() OVERRIDE;

    // Opens the given URL in a new background tab.
    void OpenNewTab(const ListValue* list_value);

    // Called on UI thread.  Adds an entry to the cache for the specified
    // hostname by posting a task to the IO thread.  Takes the host name,
    // ip address, net error code, and expiration time in days from now
    // as parameters.  If the error code indicates failure, the ip address
    // must be an empty string.
    void AddCacheEntry(const ListValue* list_value);

    // Navigates to the prerender in the background tab. This assumes that
    // there is a "Click()" function in the background tab which will navigate
    // there, and that the background tab exists at slot 1.
    void NavigateToPrerender(const ListValue* list_value);

    // Creates an incognito browser.  Once creation is complete, passes a
    // message to the Javascript test harness.
    void CreateIncognitoBrowser(const ListValue* list_value);

    // Closes an incognito browser created with CreateIncognitoBrowser.
    void CloseIncognitoBrowser(const ListValue* list_value);

    Browser* browser() {
      return net_internals_test_->browser();
    }

    NetInternalsTest* net_internals_test_;
    Browser* incognito_browser_;

    DISALLOW_COPY_AND_ASSIGN(MessageHandler);
  };

  virtual WebUIMessageHandler* GetMockMessageHandler() OVERRIDE {
    return &message_handler_;
  }

  MessageHandler message_handler_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsTest);
};

NetInternalsTest::NetInternalsTest()
    : ALLOW_THIS_IN_INITIALIZER_LIST(message_handler_(this)) {
}

NetInternalsTest::~NetInternalsTest() {
}

void NetInternalsTest::SetUpCommandLine(CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
  // Needed to test the prerender view.
  command_line->AppendSwitchASCII(switches::kPrerenderMode,
                                  switches::kPrerenderModeSwitchValueEnabled);
}

void NetInternalsTest::SetUpInProcessBrowserTestFixture() {
  // Adds libraries needed for testing, so much be first.
  WebUIBrowserTest::SetUpInProcessBrowserTestFixture();

  // Framework for net-internals tests.
  AddLibrary(FilePath(FILE_PATH_LITERAL(
                          "net_internals/net_internals_test.js")));

  // Add Javascript files needed for individual tests.
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/dns_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/hsts_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/log_util.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/log_view_painter.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/main.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/prerender_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/test_view.js")));
  AddLibrary(FilePath(FILE_PATH_LITERAL("net_internals/timeline_view.js")));
}

void NetInternalsTest::SetUpOnMainThread() {
  // Navigate to chrome://net-internals.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUINetInternalsURL));
  // Increase the memory allowed in a prerendered page above normal settings,
  // as debug builds use more memory and often go over the usual limit.
  Profile* profile = browser()->GetSelectedTabContentsWrapper()->profile();
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  prerender_manager->mutable_config().max_bytes = 1000 * 1024 * 1024;
}

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest::MessageHandler
////////////////////////////////////////////////////////////////////////////////

NetInternalsTest::MessageHandler::MessageHandler(
    NetInternalsTest* net_internals_test)
    : net_internals_test_(net_internals_test),
      incognito_browser_(NULL) {
}

void NetInternalsTest::MessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openNewTab",
      base::Bind(&NetInternalsTest::MessageHandler::OpenNewTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addCacheEntry",
      base::Bind(&NetInternalsTest::MessageHandler::AddCacheEntry,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("navigateToPrerender",
       base::Bind(&NetInternalsTest::MessageHandler::NavigateToPrerender,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createIncognitoBrowser",
       base::Bind(&NetInternalsTest::MessageHandler::CreateIncognitoBrowser,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback("closeIncognitoBrowser",
       base::Bind(&NetInternalsTest::MessageHandler::CloseIncognitoBrowser,
                  base::Unretained(this)));
}

void NetInternalsTest::MessageHandler::OpenNewTab(
    const ListValue* list_value) {
  std::string url;
  ASSERT_TRUE(list_value->GetString(0, &url));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
}

void NetInternalsTest::MessageHandler::AddCacheEntry(
    const ListValue* list_value) {
  std::string hostname;
  std::string ip_literal;
  double net_error;
  double expire_days_from_now;
  ASSERT_TRUE(list_value->GetString(0, &hostname));
  ASSERT_TRUE(list_value->GetString(1, &ip_literal));
  ASSERT_TRUE(list_value->GetDouble(2, &net_error));
  ASSERT_TRUE(list_value->GetDouble(3, &expire_days_from_now));
  ASSERT_TRUE(browser());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddCacheEntryOnIOThread,
                 make_scoped_refptr(browser()->profile()->GetRequestContext()),
                 hostname,
                 ip_literal,
                 static_cast<int>(net_error),
                 static_cast<int>(expire_days_from_now)));
}

void NetInternalsTest::MessageHandler::NavigateToPrerender(
    const ListValue* list_value) {
  RenderViewHost* host = browser()->GetTabContentsAt(1)->GetRenderViewHost();
  host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16("Click()"));
}

void NetInternalsTest::MessageHandler::CreateIncognitoBrowser(
    const ListValue* list_value) {
  ASSERT_FALSE(incognito_browser_);
  incognito_browser_ = net_internals_test_->CreateIncognitoBrowser();

  // Tell the test harness that creation is complete.
  StringValue command_value("onIncognitoBrowserCreatedForTest");
  web_ui()->CallJavascriptFunction("g_browser.receive", command_value);
}

void NetInternalsTest::MessageHandler::CloseIncognitoBrowser(
    const ListValue* list_value) {
  ASSERT_TRUE(incognito_browser_);
  incognito_browser_->CloseAllTabs();
  // Closing all a Browser's tabs will ultimately result in its destruction,
  // thought it may not have been destroyed yet.
  incognito_browser_ = NULL;
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
// main.js (Also tests status_view.js)
////////////////////////////////////////////////////////////////////////////////

// Checks tabs initialization and switching between tabs.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTourTabs) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTourTabs"));
}

// Checks pressing the stop capturing button.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsStopCapturing) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsStopCapturing"));
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
// timeline_view.js
////////////////////////////////////////////////////////////////////////////////

// TODO(mmenke):  Add tests for labels and DataSeries.

// Tests setting and updating range.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewRange) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewRange"));
}

// Tests using the scroll bar.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewScrollbar) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewScrollbar"));
}

// Tests case of having no events.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewNoEvents) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewNoEvents"));
}

// Dumps a log file to memory, modifies its events, loads it again, and
// makes sure the range is correctly set and not automatically updated.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewLoadLog) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewLoadLog"));
}

// Zooms out twice, and then zooms in once.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewZoomOut) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewZoomOut"));
}

// Zooms in as much as allowed, and zooms out once.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewZoomIn) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewZoomIn"));
}

// Tests case of all events having the same time.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsTimelineViewDegenerate) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsTimelineViewDegenerate"));
}

////////////////////////////////////////////////////////////////////////////////
// dns_view.js
////////////////////////////////////////////////////////////////////////////////

// Adds a successful lookup to the DNS cache, then clears the cache.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDnsViewSuccess) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsDnsViewSuccess"));
}

// Adds a failed lookup to the DNS cache, then clears the cache.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDnsViewFail) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsDnsViewFail"));
}

// Adds an expired successful lookup to the DNS cache, then clears the cache.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDnsViewExpired) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsDnsViewExpired"));
}

// Adds two entries to the DNS cache, clears the cache, and then repeats.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDnsViewAddTwoTwice) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsDnsViewAddTwoTwice"));
}

// Makes sure that openning and then closing an incognito window clears the
// DNS cache.  To keep things simple, we add a fake cache entry ourselves,
// rather than having the incognito browser create one.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsDnsViewIncognitoClears) {
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsDnsViewIncognitoClears"));
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
  GURL prerender_url = test_server()->GetURL("files/title1.html");
  GURL loader_url = CreatePrerenderLoaderUrl(prerender_url);
  ConstValueVector args;
  args.push_back(Value::CreateStringValue(prerender_url.spec()));
  args.push_back(Value::CreateStringValue(loader_url.spec()));
  args.push_back(Value::CreateBooleanValue(true));
  args.push_back(Value::CreateStringValue(
      prerender::NameFromFinalStatus(prerender::FINAL_STATUS_USED)));
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsPrerenderView", args));
}

// Prerender a page that is expected to fail.
IN_PROC_BROWSER_TEST_F(NetInternalsTest, NetInternalsPrerenderViewFail) {
  ASSERT_TRUE(test_server()->Start());
  GURL prerender_url = test_server()->GetURL("files/download-test1.lib");
  GURL loader_url = CreatePrerenderLoaderUrl(prerender_url);
  ConstValueVector args;
  args.push_back(Value::CreateStringValue(prerender_url.spec()));
  args.push_back(Value::CreateStringValue(loader_url.spec()));
  args.push_back(Value::CreateBooleanValue(false));
  args.push_back(Value::CreateStringValue(
      prerender::NameFromFinalStatus(prerender::FINAL_STATUS_DOWNLOAD)));
  EXPECT_TRUE(RunJavascriptAsyncTest("netInternalsPrerenderView", args));
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
