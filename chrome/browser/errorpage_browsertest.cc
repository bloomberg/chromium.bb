// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/google/google_profile_helper.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
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
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::NavigationController;
using content::URLRequestFailedJob;
using net::URLRequestTestJob;

namespace {

// Returns true if |text| is displayed on the page |browser| is currently
// displaying.  Uses "innerText", so will miss hidden text, and whitespace
// space handling may be weird.
bool WARN_UNUSED_RESULT IsDisplayingText(Browser* browser,
                                         const std::string& text) {
  std::string command = base::StringPrintf(
      "var textContent = document.body.innerText;"
      "var hasText = textContent.indexOf('%s') >= 0;"
      "domAutomationController.send(hasText);",
      text.c_str());
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(), command, &result));
  return result;
}

// Expands the more box on the currently displayed error page.
void ToggleHelpBox(Browser* browser) {
  EXPECT_TRUE(content::ExecuteScript(
      browser->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('details-button').click();"));
}

// Returns true if |browser| is displaying the text representation of
// |error_code| on the current page.
bool WARN_UNUSED_RESULT IsDisplayingNetError(Browser* browser,
                                             net::Error error_code) {
  return IsDisplayingText(browser, net::ErrorToShortString(error_code));
}

// Checks that the local error page is being displayed, without remotely
// retrieved navigation corrections, and with the specified error code.
void ExpectDisplayingLocalErrorPage(Browser* browser, net::Error error_code) {
  // Expand the help box so innerText will include text below the fold.
  ToggleHelpBox(browser);

  EXPECT_TRUE(IsDisplayingNetError(browser, error_code));

  // Locally generated error pages should not have navigation corrections.
  EXPECT_FALSE(IsDisplayingText(browser, "http://correction1/"));
  EXPECT_FALSE(IsDisplayingText(browser, "http://correction2/"));

  // Locally generated error pages should not have a populated search box.
  bool search_box_populated = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(),
      "var searchText = document.getElementById('search-box').value;"
          "domAutomationController.send(searchText == 'search query');",
      &search_box_populated));
  EXPECT_FALSE(search_box_populated);
}

// Checks that an error page with information retrieved from the navigation
// correction service is being displayed, with the specified specified error
// code.
void ExpectDisplayingNavigationCorrections(Browser* browser,
                                           net::Error error_code) {
  // Expand the help box so innerText will include text below the fold.
  ToggleHelpBox(browser);

  EXPECT_TRUE(IsDisplayingNetError(browser, error_code));

  // Check that the mock navigation corrections are displayed.
  EXPECT_TRUE(IsDisplayingText(browser, "http://correction1/"));
  EXPECT_TRUE(IsDisplayingText(browser, "http://correction2/"));

  // Check that the search box is populated correctly.
  bool search_box_populated = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(),
      "var searchText = document.getElementById('search-box').value;"
          "domAutomationController.send(searchText == 'search query');",
      &search_box_populated));
  EXPECT_TRUE(search_box_populated);
}

std::string GetLoadStaleButtonLabel() {
  return l10n_util::GetStringUTF8(IDS_ERRORPAGES_BUTTON_LOAD_STALE);
}

void AddInterceptorForURL(
    const GURL& url,
    scoped_ptr<net::URLRequestInterceptor> handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, handler.Pass());
}

// An interceptor that fails a configurable number of requests, then succeeds
// all requests after that, keeping count of failures and successes.
class FailFirstNRequestsInterceptor : public net::URLRequestInterceptor {
 public:
  explicit FailFirstNRequestsInterceptor(int requests_to_fail)
      : requests_(0), failures_(0), requests_to_fail_(requests_to_fail) {}
  virtual ~FailFirstNRequestsInterceptor() {}

  // net::URLRequestInterceptor implementation
  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  // These are mutable because MaybeCreateJob is const but we want this state
  // for testing.
  mutable int requests_;
  mutable int failures_;
  int requests_to_fail_;

  DISALLOW_COPY_AND_ASSIGN(FailFirstNRequestsInterceptor);
};

// An interceptor that serves LinkDoctor responses.  It also allows waiting
// until a certain number of requests have been sent.
// TODO(mmenke):  Wait until responses have been received instead.
class LinkDoctorInterceptor : public net::URLRequestInterceptor {
 public:
  LinkDoctorInterceptor() : num_requests_(0),
                            requests_to_wait_for_(-1),
                            weak_factory_(this) {
  }

  virtual ~LinkDoctorInterceptor() {}

  // net::URLRequestInterceptor implementation
  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&LinkDoctorInterceptor::RequestCreated,
                   weak_factory_.GetWeakPtr()));

    base::FilePath root_http;
    PathService::Get(chrome::DIR_TEST_DATA, &root_http);
    return new content::URLRequestMockHTTPJob(
        request, network_delegate,
        root_http.AppendASCII("mock-link-doctor.json"));
  }

  void WaitForRequests(int requests_to_wait_for) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK_EQ(-1, requests_to_wait_for_);
    DCHECK(!run_loop_);

    if (requests_to_wait_for >= num_requests_)
      return;

    requests_to_wait_for_ = requests_to_wait_for;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset();
    requests_to_wait_for_ = -1;
    EXPECT_EQ(num_requests_, requests_to_wait_for);
  }

  // It is up to the caller to wait until all relevant requests has been
  // created, either through calling WaitForRequests or some other manner,
  // before calling this method.
  int num_requests() const {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return num_requests_;
  }

 private:
  void RequestCreated() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    num_requests_++;
    if (num_requests_ == requests_to_wait_for_)
      run_loop_->Quit();
  }

  // These are only used on the UI thread.
  int num_requests_;
  int requests_to_wait_for_;
  scoped_ptr<base::RunLoop> run_loop_;

  // This prevents any risk of flake if any test doesn't wait for a request
  // it sent.  Mutable so it can be accessed from a const function.
  mutable base::WeakPtrFactory<LinkDoctorInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LinkDoctorInterceptor);
};

void InstallMockInterceptors(
    const GURL& search_url,
    scoped_ptr<net::URLRequestInterceptor> link_doctor_interceptor) {
  chrome_browser_net::SetUrlRequestMocksEnabled(true);

  AddInterceptorForURL(google_util::LinkDoctorBaseURL(),
                       link_doctor_interceptor.Pass());

  // Add a mock for the search engine the error page will use.
  base::FilePath root_http;
  PathService::Get(chrome::DIR_TEST_DATA, &root_http);
  content::URLRequestMockHTTPJob::AddHostnameToFileHandler(
      search_url.host(), root_http.AppendASCII("title3.html"));
}

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  ErrorPageTest() : link_doctor_interceptor_(NULL) {}
  virtual ~ErrorPageTest() {}

  // Navigates the active tab to a mock url created for the file at |file_path|.
  // Needed for StaleCacheStatus and StaleCacheStatusFailedCorrections tests.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableOfflineLoadStaleCache);
  }

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

  void GoBackAndWaitForNavigations(int num_navigations) {
    NavigateHistory(num_navigations, HISTORY_NAVIGATE_BACK);
  }

  void GoForwardAndWaitForNavigations(int num_navigations) {
    NavigateHistory(num_navigations, HISTORY_NAVIGATE_FORWARD);
  }

  // Confirms that the javascript variable indicating whether or not we have
  // a stale copy in the cache has been set to |expected|, and that the
  // stale load button is or isn't there based on the same expectation.
  testing::AssertionResult ProbeStaleCopyValue(bool expected) {
    const char* js_cache_probe =
        "try {\n"
        "    domAutomationController.send(\n"
        "        'staleLoadButton' in templateData ? 'yes' : 'no');\n"
        "} catch (e) {\n"
        "    domAutomationController.send(e.message);\n"
        "}\n";

    std::string result;
    bool ret =
        content::ExecuteScriptAndExtractString(
            browser()->tab_strip_model()->GetActiveWebContents(),
            js_cache_probe,
            &result);
    if (!ret) {
      return testing::AssertionFailure()
          << "Failing return from ExecuteScriptAndExtractString.";
    }

    if ((expected && "yes" == result) || (!expected && "no" == result))
      return testing::AssertionSuccess();

    return testing::AssertionFailure() << "Cache probe result is " << result;
  }

  testing::AssertionResult ReloadStaleCopyFromCache() {
    const char* js_reload_script =
        "try {\n"
        "    document.getElementById('stale-load-button').click();\n"
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

  LinkDoctorInterceptor* link_doctor_interceptor() {
    return link_doctor_interceptor_;
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    link_doctor_interceptor_ = new LinkDoctorInterceptor();
    scoped_ptr<net::URLRequestInterceptor> owned_interceptor(
        link_doctor_interceptor_);
    // Ownership of the |interceptor_| is passed to an object the IO thread, but
    // a pointer is kept in the test fixture.  As soon as anything calls
    // URLRequestFilter::ClearHandlers(), |interceptor_| can become invalid.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstallMockInterceptors,
                   google_util::GetGoogleSearchURL(
                       google_profile_helper::GetGoogleHomePageURL(
                           browser()->profile())),
                   base::Passed(&owned_interceptor)));
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

    NavigateHistory(num_navigations, direction);

    EXPECT_EQ(title_watcher.WaitAndGetTitle(),
              base::ASCIIToUTF16(expected_title));
  }

  void NavigateHistory(int num_navigations,
                       HistoryNavigationDirection direction) {
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
  }

  LinkDoctorInterceptor* link_doctor_interceptor_;
};

class TestFailProvisionalLoadObserver : public content::WebContentsObserver {
 public:
  explicit TestFailProvisionalLoadObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  virtual ~TestFailProvisionalLoadObserver() {}

  // This method is invoked when the provisional load failed.
  virtual void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description) OVERRIDE {
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

// Test that a DNS error occuring in the main frame redirects to an error page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_Basic) {
  // The first navigation should fail, and the second one should be the error
  // page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack1) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());

  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2AndForward) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());

  GoBackAndWaitForTitle("Title Of Awesomeness", 1);

  GoForwardAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(3, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2Forward2) {
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());

  GoBackAndWaitForTitle("Title Of More Awesomeness", 1);

  GoForwardAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(3, link_doctor_interceptor()->num_requests());

  GoForwardAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(3, link_doctor_interceptor()->num_requests());
}

// Test that the search button on a DNS error page works.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_DoSearch) {
  // The first navigation should fail, and the second one should be the error
  // page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Do a search and make sure the browser ends up at the right page.
  content::TestNavigationObserver nav_observer(web_contents, 1);
  content::TitleWatcher title_watcher(
      web_contents,
      base::ASCIIToUTF16("Title Of More Awesomeness"));
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16("document.getElementById('search-button').click();"));
  nav_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Title Of More Awesomeness"),
            title_watcher.WaitAndGetTitle());

  // There should have been another Link Doctor request, for tracking purposes.
  // Have to wait for it, since the search page does not depend on having
  // sent the tracking request.
  link_doctor_interceptor()->WaitForRequests(2);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());

  // Check the path and query string.
  std::string url;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
                  browser()->tab_strip_model()->GetActiveWebContents(),
                  "domAutomationController.send(window.location.href);",
                  &url));
  EXPECT_EQ("/search", GURL(url).path());
  EXPECT_EQ("q=search%20query", GURL(url).query());

  // Go back to the error page, to make sure the history is correct.
  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(3, link_doctor_interceptor()->num_requests());
}

// Test that the reload button on a DNS error page works.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_DoReload) {
  // The first navigation should fail, and the second one should be the error
  // page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Clicking the reload button should load the error page again, and there
  // should be two commits, as before.
  content::TestNavigationObserver nav_observer(web_contents, 2);
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer.Wait();
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);

  // There should have two more requests to the correction service:  One for the
  // new error page, and one for tracking purposes.  Have to make sure to wait
  // for the tracking request, since the new error page does not depend on it.
  link_doctor_interceptor()->WaitForRequests(3);
  EXPECT_EQ(3, link_doctor_interceptor()->num_requests());
}

// Test that clicking links on a DNS error page works.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_DoClickLink) {
  // The first navigation should fail, and the second one should be the error
  // page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Simulate a click on a link.

  content::TitleWatcher title_watcher(
      web_contents,
      base::ASCIIToUTF16("Title Of Awesomeness"));
  std::string link_selector =
      "document.querySelector('a[href=\"http://mock.http/title2.html\"]')";
  // The tracking request is triggered by onmousedown, so it catches middle
  // mouse button clicks, as well as left clicks.
  web_contents->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16(link_selector + ".onmousedown();"));
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16(link_selector + ".click();"));
  EXPECT_EQ(base::ASCIIToUTF16("Title Of Awesomeness"),
            title_watcher.WaitAndGetTitle());

  // There should have been a tracking request to the correction service.  Have
  // to make sure to wait the tracking request, since the new page does not
  // depend on it.
  link_doctor_interceptor()->WaitForRequests(2);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in an iframe does not result in showing
// navigation corrections.
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
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
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
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
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
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
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
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
}

// Checks that navigation corrections are not loaded when we receive an actual
// 404 page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Page404) {
  NavigateToURLAndWaitForTitle(
      content::URLRequestMockHTTPJob::GetMockUrl(
          base::FilePath(FILE_PATH_LITERAL("page404.html"))),
      "SUCCESS",
      1);
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
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
                 net::ERR_FAILED));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      // With no navigation corrections to load, there's only one navigation.
      browser(), test_url, 1);
  EXPECT_TRUE(ProbeStaleCopyValue(true));
  EXPECT_TRUE(IsDisplayingText(browser(), GetLoadStaleButtonLabel()));
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
  EXPECT_FALSE(IsDisplayingText(browser(), GetLoadStaleButtonLabel()));
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
}

class ErrorPageAutoReloadTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
  }

  void InstallInterceptor(const GURL& url, int requests_to_fail) {
    interceptor_ = new FailFirstNRequestsInterceptor(requests_to_fail);
    scoped_ptr<net::URLRequestInterceptor> owned_interceptor(interceptor_);

    // Tests don't need to wait for this task to complete before using the
    // filter; any requests that might be affected by it will end up in the IO
    // thread's message loop after this posted task anyway.
    //
    // Ownership of the interceptor is passed to an object the IO thread, but a
    // pointer is kept in the test fixture.  As soon as anything calls
    // URLRequestFilter::ClearHandlers(), |interceptor_| can become invalid.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&AddInterceptorForURL, url,
                   base::Passed(&owned_interceptor)));
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

  FailFirstNRequestsInterceptor* interceptor() {
    return interceptor_;
  }

 private:
  FailFirstNRequestsInterceptor* interceptor_;
};

IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, AutoReload) {
  GURL test_url("http://error.page.auto.reload");
  const int kRequestsToFail = 2;
  InstallInterceptor(test_url, kRequestsToFail);
  NavigateToURLAndWaitForTitle(test_url, "Test One", kRequestsToFail + 1);
  // Note that the interceptor updates these variables on the IO thread,
  // but this function reads them on the main thread. The requests have to be
  // created (on the IO thread) before NavigateToURLAndWaitForTitle returns or
  // this becomes racey.
  EXPECT_EQ(kRequestsToFail, interceptor()->failures());
  EXPECT_EQ(kRequestsToFail + 1, interceptor()->requests());
}

// Interceptor that fails all requests with net::ERR_ADDRESS_UNREACHABLE.
class AddressUnreachableInterceptor : public net::URLRequestInterceptor {
 public:
  AddressUnreachableInterceptor() {}
  virtual ~AddressUnreachableInterceptor() {}

  // net::URLRequestInterceptor:
  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new URLRequestFailedJob(request,
                                   network_delegate,
                                   net::ERR_ADDRESS_UNREACHABLE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressUnreachableInterceptor);
};

// A test fixture that returns ERR_ADDRESS_UNREACHABLE for all navigation
// correction requests.  ERR_NAME_NOT_RESOLVED is more typical, but need to use
// a different error for the correction service and the original page to
// validate the right page is being displayed.
class ErrorPageNavigationCorrectionsFailTest : public ErrorPageTest {
 public:
  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageNavigationCorrectionsFailTest::AddFilters));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageNavigationCorrectionsFailTest::RemoveFilters));
  }

 private:
  // Adds a filter that causes all correction service requests to fail with
  // ERR_ADDRESS_UNREACHABLE.
  //
  // Also adds the content::URLRequestFailedJob filter.
  static void AddFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    content::URLRequestFailedJob::AddUrlHandler();

    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        google_util::LinkDoctorBaseURL(),
        scoped_ptr<net::URLRequestInterceptor>(
            new AddressUnreachableInterceptor()));
  }

  static void RemoveFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }
};

// Make sure that when corrections fail to load, the network error page is
// successfully loaded.
IN_PROC_BROWSER_TEST_F(ErrorPageNavigationCorrectionsFailTest,
                       FetchCorrectionsFails) {
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED),
      2);

  // Verify that the expected error page is being displayed.
  ExpectDisplayingLocalErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);
}

// Checks that when an error occurs and a corrections fail to load, the stale
// cache status of the page is correctly transferred, and we can load the
// stale copy from the javascript.  Most logic copied from StaleCacheStatus
// above.
IN_PROC_BROWSER_TEST_F(ErrorPageNavigationCorrectionsFailTest,
                       StaleCacheStatusFailedCorrections) {
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
  EXPECT_TRUE(IsDisplayingText(browser(), GetLoadStaleButtonLabel()));
  EXPECT_TRUE(ProbeStaleCopyValue(true));

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
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetLoadStaleButtonLabel()));
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

  virtual void TearDownOnMainThread() OVERRIDE {
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
  // ERR_UNSAFE_PORT will not trigger navigation corrections.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrlForHostname(net::ERR_UNSAFE_PORT,
                                                     kHostname),
      1);

  ToggleHelpBox(browser());
  EXPECT_TRUE(IsDisplayingText(browser(), kHostnameJSUnicode));
}

}  // namespace
