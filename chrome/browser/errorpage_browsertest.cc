// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/net/url_request_failed_job.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"

using content::BrowserThread;
using content::NavigationController;
using content::URLRequestFailedJob;
using net::URLRequestJobFactory;
using net::URLRequestTestJob;

namespace {

// A protocol handler that fails a configurable number of requests, then
// succeeds all requests after that, keeping count of failures and successes.
class FailFirstNRequestsProtocolHandler
    : public URLRequestJobFactory::ProtocolHandler {
 public:
  FailFirstNRequestsProtocolHandler(const GURL& url, int requests_to_fail)
      : url_(url), requests_(0), failures_(0),
        requests_to_fail_(requests_to_fail) {}
  virtual ~FailFirstNRequestsProtocolHandler() {}

  // This method deliberately violates pointer ownership rules:
  // AddUrlProtocolHandler() takes a scoped_ptr, taking ownership of the
  // supplied ProtocolHandler (i.e., |this|), but also having the caller retain
  // a pointer to |this| so the caller can use the requests() and failures()
  // accessors.
  void AddUrlHandler() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    scoped_ptr<URLRequestJobFactory::ProtocolHandler> scoped_handler(this);
    net::URLRequestFilter::GetInstance()->AddUrlProtocolHandler(
        url_,
        scoped_handler.Pass());
  }

  // net::URLRequestJobFactory::ProtocolHandler implementation
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    DCHECK_EQ(url_, request->url());
    requests_++;
    if (failures_ < requests_to_fail_) {
      failures_++;
      // Note: net::ERR_CONNECTION_RESET does not summon the Link Doctor; see
      // NetErrorHelperCore::GetErrorPageURL.
      return new URLRequestFailedJob(request,
                                     network_delegate,
                                     net::ERR_CONNECTION_RESET);
    } else {
      return new URLRequestTestJob(request, network_delegate,
                                   URLRequestTestJob::test_headers(),
                                   URLRequestTestJob::test_data_1(),
                                   true);
    }
  }

  int requests() const { return requests_; }
  int failures() const { return failures_; }

 private:
  const GURL url_;
  // These are mutable because MaybeCreateJob is const but we want this state
  // for testing.
  mutable int requests_;
  mutable int failures_;
  int requests_to_fail_;
};

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  // Navigates the active tab to a mock url created for the file at |file_path|.
  void NavigateToFileURL(const base::FilePath::StringType& file_path) {
    ui_test_utils::NavigateToURL(
        browser(),
        content::URLRequestMockHTTPJob::GetMockUrl(base::FilePath(file_path)));
  }

  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  // Navigates back in the history and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void GoBackAndWaitForTitle(const std::string& expected_title,
                             int num_navigations) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   num_navigations,
                                   HISTORY_NAVIGATE_BACK);
  }

  // Navigates forward in the history and waits for |num_navigations| to occur,
  // and the title to change to |expected_title|.
  void GoForwardAndWaitForTitle(const std::string& expected_title,
                                int num_navigations) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   num_navigations,
                                   HISTORY_NAVIGATE_FORWARD);
  }

  // Confirms that the javascript variable indicating whether or not we have
  // a stale copy in the cache has been set to |expected|.
  bool ProbeStaleCopyValue(bool expected) {
    const char* js_cache_probe =
        "try {\n"
        "    domAutomationController.send(\n"
        "        templateData.staleCopyInCache ? 'yes' : 'no');\n"
        "} catch (e) {\n"
        "    domAutomationController.send(e.message);\n"
        "}\n";

    std::string result;
    bool ret =
        content::ExecuteScriptAndExtractString(
            browser()->tab_strip_model()->GetActiveWebContents(),
            js_cache_probe,
            &result);
    EXPECT_TRUE(ret);
    if (!ret)
      return false;
    EXPECT_EQ(expected ? "yes" : "no", result);
    return ((expected ? "yes" : "no") == result);
  }

  testing::AssertionResult ReloadStaleCopyFromCache() {
    const char* js_reload_script =
        "try {\n"
        "    errorCacheLoad.reloadStaleInstance();\n"
        "    domAutomationController.send('success');\n"
        "} catch (e) {\n"
        "    domAutomationController.send(e.message);\n"
        "}\n";

    std::string result;
    bool ret = content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        js_reload_script,
        &result);
    EXPECT_TRUE(ret);
    if (!ret)
      return testing::AssertionFailure();
    return ("success" == result ? testing::AssertionSuccess() :
            (testing::AssertionFailure() << "Exception message is " << result));
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  // Returns a GURL that results in a DNS error.
  GURL GetDnsErrorURL() const {
    return URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  }

 private:
  // Navigates the browser the indicated direction in the history and waits for
  // |num_navigations| to occur and the title to change to |expected_title|.
  void NavigateHistoryAndWaitForTitle(const std::string& expected_title,
                                      int num_navigations,
                                      HistoryNavigationDirection direction) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    content::TestNavigationObserver test_navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        num_navigations);
    if (direction == HISTORY_NAVIGATE_BACK) {
      chrome::GoBack(browser(), CURRENT_TAB);
    } else if (direction == HISTORY_NAVIGATE_FORWARD) {
      chrome::GoForward(browser(), CURRENT_TAB);
    } else {
      FAIL();
    }
    test_navigation_observer.Wait();

    EXPECT_EQ(title_watcher.WaitAndGetTitle(),
              base::ASCIIToUTF16(expected_title));
  }
};

class TestFailProvisionalLoadObserver : public content::WebContentsObserver {
 public:
  explicit TestFailProvisionalLoadObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  virtual ~TestFailProvisionalLoadObserver() {}

  // This method is invoked when the provisional load failed.
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE {
    fail_url_ = validated_url;
  }

  const GURL& fail_url() const { return fail_url_; }

 private:
  GURL fail_url_;

  DISALLOW_COPY_AND_ASSIGN(TestFailProvisionalLoadObserver);
};

void InterceptNetworkTransactions(net::URLRequestContextGetter* getter,
                                  net::Error error) {
  DCHECK(content::BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::HttpCache* cache(
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache());
  DCHECK(cache);
  scoped_ptr<net::HttpTransactionFactory> factory(
      new net::FailingHttpTransactionFactory(cache->GetSession(), error));
  // Throw away old version; since this is a a browser test, we don't
  // need to restore the old state.
  cache->SetHttpNetworkTransactionFactoryForTesting(factory.Pass());
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_Basic DISABLED_DNSError_Basic
#else
#define MAYBE_DNSError_Basic DNSError_Basic
#endif
// Test that a DNS error occuring in the main frame redirects to an error page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_DNSError_Basic) {
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack1 DISABLED_DNSError_GoBack1
#else
#define MAYBE_DNSError_GoBack1 DNSError_GoBack1
#endif

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_DNSError_GoBack1) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack2 DISABLED_DNSError_GoBack2
#else
#define MAYBE_DNSError_GoBack2 DNSError_GoBack2
#endif
// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack2AndForward DISABLED_DNSError_GoBack2AndForward
#else
#define MAYBE_DNSError_GoBack2AndForward DNSError_GoBack2AndForward
#endif
// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2AndForward) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);

  GoForwardAndWaitForTitle("Mock Link Doctor", 2);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack2Forward2 DISABLED_DNSError_GoBack2Forward2
#else
#define MAYBE_DNSError_GoBack2Forward2 DNSError_GoBack2Forward2
#endif
// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2Forward2) {
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of More Awesomeness", 1);

  GoForwardAndWaitForTitle("Mock Link Doctor", 2);
  GoForwardAndWaitForTitle("Title Of Awesomeness", 1);
}

// Test that a DNS error occuring in an iframe.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, IFrameDNSError_Basic) {
  NavigateToURLAndWaitForTitle(
      content::URLRequestMockHTTPJob::GetMockUrl(
          base::FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))),
      "Blah",
      1);
  // We expect to have two history entries, since we started off with navigation
  // to "about:blank" and then navigated to "iframe_dns_error.html".
  EXPECT_EQ(2,
      browser()->tab_strip_model()->GetActiveWebContents()->
          GetController().GetEntryCount());
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
#if defined(OS_WIN)
#define MAYBE_IFrameDNSError_GoBack DISABLED_IFrameDNSError_GoBack
#else
#define MAYBE_IFrameDNSError_GoBack IFrameDNSError_GoBack
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_IFrameDNSError_GoBack) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToFileURL(FILE_PATH_LITERAL("iframe_dns_error.html"));
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
//
// This fails on linux_aura bringup: http://crbug.com/163931
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))
#define MAYBE_IFrameDNSError_GoBackAndForward DISABLED_IFrameDNSError_GoBackAndForward
#else
#define MAYBE_IFrameDNSError_GoBackAndForward IFrameDNSError_GoBackAndForward
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_IFrameDNSError_GoBackAndForward) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToFileURL(FILE_PATH_LITERAL("iframe_dns_error.html"));
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  GoForwardAndWaitForTitle("Blah", 1);
}

// Test that a DNS error occuring in an iframe, once the main document is
// completed loading, does not result in an additional session history entry.
// To ensure that the main document has completed loading, JavaScript is used to
// inject an iframe after loading is done.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, IFrameDNSError_JavaScript) {
  content::WebContents* wc =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL fail_url =
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);

  // Load a regular web page, in which we will inject an iframe.
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  // We expect to have two history entries, since we started off with navigation
  // to "about:blank" and then navigated to "title2.html".
  EXPECT_EQ(2, wc->GetController().GetEntryCount());

  std::string script = "var frame = document.createElement('iframe');"
                       "frame.src = '" + fail_url.spec() + "';"
                       "document.body.appendChild(frame);";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScript(base::ASCIIToUTF16(script));
    load_observer.Wait();

    // Ensure we saw the expected failure.
    EXPECT_EQ(fail_url, fail_observer.fail_url());

    // Failed initial navigation of an iframe shouldn't be adding any history
    // entries.
    EXPECT_EQ(2, wc->GetController().GetEntryCount());
  }

  // Do the same test, but with an iframe that doesn't have initial URL
  // assigned.
  script = "var frame = document.createElement('iframe');"
           "frame.id = 'target_frame';"
           "document.body.appendChild(frame);";
  {
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScript(base::ASCIIToUTF16(script));
    load_observer.Wait();
  }

  script = "var f = document.getElementById('target_frame');"
           "f.src = '" + fail_url.spec() + "';";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScript(base::ASCIIToUTF16(script));
    load_observer.Wait();

    EXPECT_EQ(fail_url, fail_observer.fail_url());
    EXPECT_EQ(2, wc->GetController().GetEntryCount());
  }
}

// Checks that the Link Doctor is not loaded when we receive an actual 404 page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Page404) {
  NavigateToURLAndWaitForTitle(
      content::URLRequestMockHTTPJob::GetMockUrl(
          base::FilePath(FILE_PATH_LITERAL("page404.html"))),
      "SUCCESS",
      1);
}

// Checks that when an error occurs, the stale cache status of the page
// is correctly transferred, and that stale cached copied can be loaded
// from the javascript.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, StaleCacheStatus) {
  ASSERT_TRUE(test_server()->Start());
  // Load cache with entry with "nocache" set, to create stale
  // cache.
  GURL test_url(test_server()->GetURL("files/nocache.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      browser()->profile()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InterceptNetworkTransactions, url_request_context_getter,
                 // Note that we can't use an error that'll invoke the link
                 // doctor.  In normal network error conditions that would
                 // work (because the link doctor fetch would also fail,
                 // putting us back in the main offline path), but
                 // SetUrlRequestMocksEnabled() has also specfied a link
                 // doctor mock, which will be accessible because it
                 // won't go through the network cache.
                 net::ERR_FAILED));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      // With no link doctor load, there's only one navigation.
      browser(), test_url, 1);
  EXPECT_TRUE(ProbeStaleCopyValue(true));
  EXPECT_NE(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Confirm that loading the stale copy from the cache works.
  content::TestNavigationObserver same_tab_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_TRUE(ReloadStaleCopyFromCache());
  same_tab_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Clear the cache and reload the same URL; confirm the error page is told
  // that there is no cached copy.
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(browser()->profile());
  remover->Remove(BrowsingDataRemover::REMOVE_CACHE,
                  BrowsingDataHelper::UNPROTECTED_WEB);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 1);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
}

class ErrorPageAutoReloadTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
  }

  void InstallProtocolHandler(const GURL& url, int requests_to_fail) {
    protocol_handler_ = new FailFirstNRequestsProtocolHandler(
        url,
        requests_to_fail);
    // Tests don't need to wait for this task to complete before using the
    // filter; any requests that might be affected by it will end up in the IO
    // thread's message loop after this posted task anyway.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageAutoReloadTest::AddFilters,
                   base::Unretained(this)));
  }

  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  FailFirstNRequestsProtocolHandler* protocol_handler() {
    return protocol_handler_;
  }

 private:
  void AddFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // Note: in theory, AddUrlHandler gives ownership of |protocol_handler_| to
    // URLRequestFilter. As soon as anything calls
    // URLRequestFilter::ClearHandlers(), |protocol_handler_| can become
    // invalid.
    protocol_handler_->AddUrlHandler();
  }

  FailFirstNRequestsProtocolHandler* protocol_handler_;
};

IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, AutoReload) {
  GURL test_url("http://error.page.auto.reload");
  const int kRequestsToFail = 2;
  InstallProtocolHandler(test_url, kRequestsToFail);
  NavigateToURLAndWaitForTitle(test_url, "Test One", kRequestsToFail + 1);
  // Note that the protocol handler updates these variables on the IO thread,
  // but this function reads them on the main thread. The requests have to be
  // created (on the IO thread) before NavigateToURLAndWaitForTitle returns or
  // this becomes racey.
  EXPECT_EQ(kRequestsToFail, protocol_handler()->failures());
  EXPECT_EQ(kRequestsToFail + 1, protocol_handler()->requests());
}

// Returns Javascript code that executes plain text search for the page.
// Pass into content::ExecuteScriptAndExtractBool as |script| parameter.
std::string GetTextContentContainsStringScript(
    const std::string& value_to_search) {
  return base::StringPrintf(
      "var textContent = document.body.textContent;"
      "var hasError = textContent.indexOf('%s') >= 0;"
      "domAutomationController.send(hasError);",
      value_to_search.c_str());
}

// Protocol handler that fails all requests with net::ERR_ADDRESS_UNREACHABLE.
class AddressUnreachableProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  AddressUnreachableProtocolHandler() {}
  virtual ~AddressUnreachableProtocolHandler() {}

  // net::URLRequestJobFactory::ProtocolHandler:
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new URLRequestFailedJob(request,
                                   network_delegate,
                                   net::ERR_ADDRESS_UNREACHABLE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressUnreachableProtocolHandler);
};

// A test fixture that returns ERR_ADDRESS_UNREACHABLE for all Link Doctor
// requests.  ERR_NAME_NOT_RESOLVED is more typical, but need to use a different
// error for the Link Doctor and the original page to validate the right page
// is being displayed.
class ErrorPageLinkDoctorFailTest : public ErrorPageTest {
 public:
  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageLinkDoctorFailTest::AddFilters));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageLinkDoctorFailTest::RemoveFilters));
  }

 private:
  // Adds a filter that causes all requests for the Link Doctor's scheme and
  // host to fail with ERR_ADDRESS_UNREACHABLE.  Since the Link Doctor adds
  // query strings, it's not enough to just fail exact matches.
  //
  // Also adds the content::URLRequestFailedJob filter.
  static void AddFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    content::URLRequestFailedJob::AddUrlHandler();

    net::URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
        google_util::LinkDoctorBaseURL().scheme(),
        google_util::LinkDoctorBaseURL().host(),
        scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            new AddressUnreachableProtocolHandler()));
  }

  static void RemoveFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }
};

// Make sure that when the Link Doctor fails to load, the network error page is
// successfully loaded.
IN_PROC_BROWSER_TEST_F(ErrorPageLinkDoctorFailTest, LinkDoctorFail) {
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED),
      2);

  // Verify that the expected error page is being displayed.  Do this by making
  // sure the original error code (ERR_NAME_NOT_RESOLVED) is displayed.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      GetTextContentContainsStringScript("ERR_NAME_NOT_RESOLVED"),
      &result));
  EXPECT_TRUE(result);
}

// Checks that when an error occurs and a link doctor load fails, the stale
// cache status of the page is correctly transferred, and we can load the
// stale copy from the javascript.  Most logic copied from StaleCacheStatus
// above.
IN_PROC_BROWSER_TEST_F(ErrorPageLinkDoctorFailTest,
                       StaleCacheStatusFailedLinkDoctor) {
  ASSERT_TRUE(test_server()->Start());
  // Load cache with entry with "nocache" set, to create stale
  // cache.
  GURL test_url(test_server()->GetURL("files/nocache.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      browser()->profile()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InterceptNetworkTransactions, url_request_context_getter,
                 net::ERR_CONNECTION_FAILED));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);
  ProbeStaleCopyValue(true);

  // Confirm that loading the stale copy from the cache works.
  content::TestNavigationObserver same_tab_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_TRUE(ReloadStaleCopyFromCache());
  same_tab_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Clear the cache and reload the same URL; confirm the error page is told
  // that there is no cached copy.
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(browser()->profile());
  remover->Remove(BrowsingDataRemover::REMOVE_CACHE,
                  BrowsingDataHelper::UNPROTECTED_WEB);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);
  ProbeStaleCopyValue(false);
}

// A test fixture that simulates failing requests for an IDN domain name.
class ErrorPageForIDNTest : public InProcessBrowserTest {
 public:
  // Target hostname in different forms.
  static const char kHostname[];
  static const char kHostnameJSUnicode[];

  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    // Clear AcceptLanguages to force punycode decoding.
    browser()->profile()->GetPrefs()->SetString(prefs::kAcceptLanguages,
                                                std::string());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageForIDNTest::AddFilters));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageForIDNTest::RemoveFilters));
  }

 private:
  static void AddFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    content::URLRequestFailedJob::AddUrlHandlerForHostname(kHostname);
  }

  static void RemoveFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }
};

const char ErrorPageForIDNTest::kHostname[] =
    "xn--d1abbgf6aiiy.xn--p1ai";
const char ErrorPageForIDNTest::kHostnameJSUnicode[] =
    "\\u043f\\u0440\\u0435\\u0437\\u0438\\u0434\\u0435\\u043d\\u0442."
    "\\u0440\\u0444";

// Make sure error page shows correct unicode for IDN.
IN_PROC_BROWSER_TEST_F(ErrorPageForIDNTest, IDN) {
  // ERR_UNSAFE_PORT will not trigger the link doctor.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrlForHostname(net::ERR_UNSAFE_PORT,
                                                     kHostname),
      1);

  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      GetTextContentContainsStringScript(kHostnameJSUnicode),
      &result));
  EXPECT_TRUE(result);
}

}  // namespace
