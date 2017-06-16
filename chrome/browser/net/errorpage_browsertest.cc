// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "components/policy/core/common/policy_types.h"
#else
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#endif
#include "components/policy/core/common/mock_configuration_policy_provider.h"

using content::BrowserThread;
using content::NavigationController;
using net::URLRequestFailedJob;
using net::URLRequestTestJob;

namespace {

// Searches for first node containing |text|, and if it finds one, searches
// through all ancestors seeing if any of them is of class "hidden". Since it
// relies on the hidden class used by network error pages, not suitable for
// general use.
bool WARN_UNUSED_RESULT IsDisplayingText(Browser* browser,
                                         const std::string& text) {
  // clang-format off
  std::string command = base::StringPrintf(R"(
    function isNodeVisible(node) {
      if (!node || node.classList.contains('hidden'))
        return false;
      if (!node.parentElement)
        return true;
      // Otherwise, we must check all parent nodes
      return isNodeVisible(node.parentElement);
    }
    var node = document.evaluate("//*[contains(text(),'%s')]", document,
      null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
    domAutomationController.send(isNodeVisible(node));
  )", text.c_str());
  // clang-format on
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

// Returns true if the diagnostics link suggestion is displayed.
bool WARN_UNUSED_RESULT IsDisplayingDiagnosticsLink(Browser* browser) {
  std::string command = base::StringPrintf(
      "var diagnose_link = document.getElementById('diagnose-link');"
      "domAutomationController.send(diagnose_link != null);");
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(), command, &result));
  return result;
}

// Checks that the local error page is being displayed, without remotely
// retrieved navigation corrections, and with the specified error string.
void ExpectDisplayingLocalErrorPage(Browser* browser,
                                    const std::string& error_string) {
  EXPECT_TRUE(IsDisplayingText(browser, error_string));

  // Locally generated error pages should not have navigation corrections.
  EXPECT_FALSE(IsDisplayingText(browser, "http://mock.http/title2.html"));

  // Locally generated error pages should not have a link with search terms.
  EXPECT_FALSE(IsDisplayingText(browser, "search query"));
}

// Checks that the local error page is being displayed, without remotely
// retrieved navigation corrections, and with the specified error code.
void ExpectDisplayingLocalErrorPage(Browser* browser, net::Error error_code) {
  ExpectDisplayingLocalErrorPage(browser, net::ErrorToShortString(error_code));
}

// Checks that an error page with information retrieved from the navigation
// correction service is being displayed, with the specified specified error
// string.
void ExpectDisplayingNavigationCorrections(Browser* browser,
                                           const std::string& error_string) {
  EXPECT_TRUE(IsDisplayingText(browser, error_string));

  // Check that the mock navigation corrections are displayed.
  EXPECT_TRUE(IsDisplayingText(browser, "http://mock.http/title2.html"));

  // Check that the search terms are displayed as a link.
  EXPECT_TRUE(IsDisplayingText(browser, "search query"));

  // The diagnostics button isn't displayed when corrections were
  // retrieved from a remote server.
  EXPECT_FALSE(IsDisplayingDiagnosticsLink(browser));
}

// Checks that an error page with information retrieved from the navigation
// correction service is being displayed, with the specified specified error
// code.
void ExpectDisplayingNavigationCorrections(Browser* browser,
                                           net::Error error_code) {
  ExpectDisplayingNavigationCorrections(browser,
                                        net::ErrorToShortString(error_code));
}

std::string GetShowSavedButtonLabel() {
  return l10n_util::GetStringUTF8(IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY);
}

void AddInterceptorForURL(const GURL& url,
                          std::unique_ptr<net::URLRequestInterceptor> handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(url,
                                                          std::move(handler));
}

// An interceptor that fails a configurable number of requests, then succeeds
// all requests after that, keeping count of failures and successes.
class FailFirstNRequestsInterceptor : public net::URLRequestInterceptor {
 public:
  explicit FailFirstNRequestsInterceptor(int requests_to_fail)
      : requests_(0), failures_(0), requests_to_fail_(requests_to_fail) {}
  ~FailFirstNRequestsInterceptor() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  ~LinkDoctorInterceptor() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&LinkDoctorInterceptor::RequestCreated,
                       weak_factory_.GetWeakPtr()));

    base::FilePath root_http;
    PathService::Get(chrome::DIR_TEST_DATA, &root_http);
    return new net::URLRequestMockHTTPJob(
        request, network_delegate,
        root_http.AppendASCII("mock-link-doctor.json"));
  }

  void WaitForRequests(int requests_to_wait_for) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return num_requests_;
  }

 private:
  void RequestCreated() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    num_requests_++;
    if (num_requests_ == requests_to_wait_for_)
      run_loop_->Quit();
  }

  // These are only used on the UI thread.
  int num_requests_;
  int requests_to_wait_for_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // This prevents any risk of flake if any test doesn't wait for a request
  // it sent.  Mutable so it can be accessed from a const function.
  mutable base::WeakPtrFactory<LinkDoctorInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LinkDoctorInterceptor);
};

void InstallMockInterceptors(
    const GURL& search_url,
    std::unique_ptr<net::URLRequestInterceptor> link_doctor_interceptor) {
  chrome_browser_net::SetUrlRequestMocksEnabled(true);

  AddInterceptorForURL(google_util::LinkDoctorBaseURL(),
                       std::move(link_doctor_interceptor));

  // Add a mock for the search engine the error page will use.
  base::FilePath root_http;
  PathService::Get(chrome::DIR_TEST_DATA, &root_http);
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      search_url.scheme(), search_url.host(),
      net::URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
          root_http.AppendASCII("title3.html")));
}

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  ErrorPageTest() : link_doctor_interceptor_(NULL) {}
  ~ErrorPageTest() override {}

  // Navigates the active tab to a mock url created for the file at |file_path|.
  // Needed for StaleCacheStatus and StaleCacheStatusFailedCorrections tests.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        error_page::switches::kShowSavedCopy,
        error_page::switches::kEnableShowSavedCopyPrimary);
  }

  // Navigates the active tab to a mock url created for the file at |path|.
  void NavigateToFileURL(const std::string& path) {
    ui_test_utils::NavigateToURL(browser(),
                                 net::URLRequestMockHTTPJob::GetMockUrl(path));
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
        "        loadTimeData.valueExists('showSavedCopyButton') ?"
        "            'yes' : 'no');\n"
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
        "    document.getElementById('show-saved-copy-button').click();\n"
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
  void SetUpOnMainThread() override {
    link_doctor_interceptor_ = new LinkDoctorInterceptor();
    std::unique_ptr<net::URLRequestInterceptor> owned_interceptor(
        link_doctor_interceptor_);
    // Ownership of the |interceptor_| is passed to an object the IO thread, but
    // a pointer is kept in the test fixture.  As soon as anything calls
    // URLRequestFilter::ClearHandlers(), |interceptor_| can become invalid.
    UIThreadSearchTermsData search_terms_data(browser()->profile());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&InstallMockInterceptors,
                       GURL(search_terms_data.GoogleBaseURLValue()),
                       base::Passed(&owned_interceptor)));
  }

  // Returns a GURL that results in a DNS error.
  GURL GetDnsErrorURL() const {
    return URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  }

  // Returns true if the platform has support for a diagnostics tool, which
  // can be launched from the error page.
  bool PlatformSupportsDiagnosticsTool() {
#if defined(OS_CHROMEOS)
    // ChromeOS uses an extension instead of a diagnostics dialog.
    return true;
#else
    return CanShowNetworkDiagnosticsDialog();
#endif
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
      chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    } else if (direction == HISTORY_NAVIGATE_FORWARD) {
      chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
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
  ~TestFailProvisionalLoadObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsErrorPage())
      fail_url_ = navigation_handle->GetURL();
  }

  const GURL& fail_url() const { return fail_url_; }

 private:
  GURL fail_url_;

  DISALLOW_COPY_AND_ASSIGN(TestFailProvisionalLoadObserver);
};

void InterceptNetworkTransactions(net::URLRequestContextGetter* getter,
                                  net::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HttpCache* cache(
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache());
  DCHECK(cache);
  std::unique_ptr<net::HttpTransactionFactory> factory(
      new net::FailingHttpTransactionFactory(cache->GetSession(), error));
  // Throw away old version; since this is a a browser test, we don't
  // need to restore the old state.
  cache->SetHttpNetworkTransactionFactoryForTesting(std::move(factory));
}

// Test an error with a file URL, and make sure it doesn't have a
// button to launch a network diagnostics tool.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, FileNotFound) {
  // Create an empty temp directory, to be sure there's no file in it.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  GURL non_existent_file_url =
      net::FilePathToFileURL(temp_dir.GetPath().AppendASCII("marmoset"));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), non_existent_file_url, 1);

  ExpectDisplayingLocalErrorPage(browser(), net::ERR_FILE_NOT_FOUND);
  // Should not request Link Doctor corrections for local errors.
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
  // Only errors on HTTP/HTTPS pages should display a diagnostics button.
  EXPECT_FALSE(IsDisplayingDiagnosticsLink(browser()));
}

// Check an network error page for ERR_FAILED. In particular, this should
// not trigger a link doctor error page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Failed) {
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), URLRequestFailedJob::GetMockHttpUrl(net::ERR_FAILED), 1);

  ExpectDisplayingLocalErrorPage(browser(), net::ERR_FAILED);
  // Should not request Link Doctor corrections for this error.
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame redirects to an error page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_Basic) {
  // The first navigation should fail and load a blank page, while it fetches
  // the Link Doctor response.  The second navigation is the Link Doctor.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack1) {
  NavigateToFileURL("title2.html");
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2) {
  NavigateToFileURL("title2.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  NavigateToFileURL("title3.html");

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());

  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(2, link_doctor_interceptor()->num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2AndForward) {
  NavigateToFileURL("title2.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  NavigateToFileURL("title3.html");

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
  NavigateToFileURL("title3.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  NavigateToFileURL("title2.html");

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

// Test that the search link on a DNS error page works.
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
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('search-link').click();"));
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
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer.Wait();
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);

  // There should have been two more requests to the correction service:  One
  // for the new error page, and one for tracking purposes.  Have to make sure
  // to wait for the tracking request, since the new error page does not depend
  // on it.
  link_doctor_interceptor()->WaitForRequests(3);
  EXPECT_EQ(3, link_doctor_interceptor()->num_requests());
}

// Test that the reload button on a DNS error page works after a same document
// navigation on the error page.  Error pages don't seem to do this, but some
// traces indicate this may actually happen.  This test may hang on regression.
IN_PROC_BROWSER_TEST_F(ErrorPageTest,
                       DNSError_DoReloadAfterSameDocumentNavigation) {
  // The first navigation should fail, and the second one should be the error
  // page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Do a same page navigation.
  content::TestNavigationObserver nav_observer1(web_contents, 1);
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.location='#';"));
  // The same page navigation counts as a single navigation as far as the
  // TestNavigationObserver is concerned.
  nav_observer1.Wait();
  // Page being displayed should not change.
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);
  // No new requests should have been issued.
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());

  // Clicking the reload button should load the error page again, and there
  // should be two commits, as before.
  content::TestNavigationObserver nav_observer2(web_contents, 2);
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer2.Wait();
  ExpectDisplayingNavigationCorrections(browser(), net::ERR_NAME_NOT_RESOLVED);

  // There should have been two more requests to the correction service:  One
  // for the new error page, and one for tracking purposes.  Have to make sure
  // to wait for the tracking request, since the new error page does not depend
  // on it.
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
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(link_selector + ".onmousedown();"));
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
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
      net::URLRequestMockHTTPJob::GetMockUrl("iframe_dns_error.html"), "Blah",
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
  NavigateToFileURL("title2.html");
  NavigateToFileURL("iframe_dns_error.html");
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
  NavigateToFileURL("title2.html");
  NavigateToFileURL("iframe_dns_error.html");
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
  NavigateToFileURL("title2.html");

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
    wc->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script));
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
    wc->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script));
    load_observer.Wait();
  }

  script = "var f = document.getElementById('target_frame');"
           "f.src = '" + fail_url.spec() + "';";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script));
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
      net::URLRequestMockHTTPJob::GetMockUrl("page404.html"), "SUCCESS", 1);
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
}

// Checks that navigation corrections are loaded in response to a 404 page with
// no body.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Empty404) {
  // The first navigation should fail and load a blank page, while it fetches
  // the Link Doctor response.  The second navigation is the Link Doctor.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      net::URLRequestMockHTTPJob::GetMockUrl("errorpage/empty404.html"), 2);
  // This depends on the non-internationalized error ID string in
  // localized_error.cc.
  ExpectDisplayingNavigationCorrections(browser(), "HTTP ERROR 404");
  EXPECT_EQ(1, link_doctor_interceptor()->num_requests());
}

// Checks that a local error page is shown in response to a 500 error page
// without a body.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Empty500) {
  NavigateToFileURL("errorpage/empty500.html");
  // This depends on the non-internationalized error ID string in
  // localized_error.cc.
  ExpectDisplayingLocalErrorPage(browser(), "HTTP ERROR 500");
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
}

// Checks that when an error occurs, the stale cache status of the page
// is correctly transferred, and that stale cached copied can be loaded
// from the javascript.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, StaleCacheStatus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Load cache with entry with "nocache" set, to create stale
  // cache.
  GURL test_url(embedded_test_server()->GetURL("/nocache.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      browser()->profile()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&InterceptNetworkTransactions,
                     base::RetainedRef(url_request_context_getter),
                     net::ERR_FAILED));

  // With no navigation corrections to load, there's only one navigation.
  ui_test_utils::NavigateToURL(browser(), test_url);
  EXPECT_TRUE(ProbeStaleCopyValue(true));
  EXPECT_TRUE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_NE(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Confirm that loading the stale copy from the cache works.
  content::TestNavigationObserver same_tab_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_TRUE(ReloadStaleCopyFromCache());
  same_tab_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Reload the same URL with a post request; confirm the error page is told
  // that there is no cached copy.
  ui_test_utils::NavigateToURLWithPost(browser(), test_url);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());

  // Clear the cache and reload the same URL; confirm the error page is told
  // that there is no cached copy.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  ui_test_utils::NavigateToURL(browser(), test_url);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_EQ(0, link_doctor_interceptor()->num_requests());
}

// Check that the easter egg is present and initialised and is not disabled.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, CheckEasterEggIsNotDisabled) {
  ui_test_utils::NavigateToURL(browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check for no disabled message container.
  std::string command = base::StringPrintf(
      "var hasDisableContainer = document.querySelectorAll('.snackbar').length;"
      "domAutomationController.send(hasDisableContainer);");
  int result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
               web_contents, command, &result));
  EXPECT_EQ(0, result);

  // Presence of the canvas container.
  command = base::StringPrintf(
    "var runnerCanvas = document.querySelectorAll('.runner-canvas').length;"
    "domAutomationController.send(runnerCanvas);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
               web_contents, command, &result));
  EXPECT_EQ(1, result);
}

class ErrorPageAutoReloadTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
  }

  void InstallInterceptor(const GURL& url, int requests_to_fail) {
    interceptor_ = new FailFirstNRequestsInterceptor(requests_to_fail);
    std::unique_ptr<net::URLRequestInterceptor> owned_interceptor(interceptor_);

    // Tests don't need to wait for this task to complete before using the
    // filter; any requests that might be affected by it will end up in the IO
    // thread's message loop after this posted task anyway.
    //
    // Ownership of the interceptor is passed to an object the IO thread, but a
    // pointer is kept in the test fixture.  As soon as anything calls
    // URLRequestFilter::ClearHandlers(), |interceptor_| can become invalid.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(&AddInterceptorForURL, url,
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

// Fails on official mac_trunk build. See crbug.com/465789.
#if defined(OFFICIAL_BUILD) && defined(OS_MACOSX)
#define MAYBE_AutoReload DISABLED_AutoReload
#else
#define MAYBE_AutoReload AutoReload
#endif
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, MAYBE_AutoReload) {
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

IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, ManualReloadNotSuppressed) {
  GURL test_url("http://error.page.auto.reload");
  const int kRequestsToFail = 3;
  InstallInterceptor(test_url, kRequestsToFail);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);

  EXPECT_EQ(2, interceptor()->failures());
  EXPECT_EQ(2, interceptor()->requests());

  ToggleHelpBox(browser());
  EXPECT_TRUE(IsDisplayingText(
      browser(), l10n_util::GetStringUTF8(
                     IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER)));

  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer.Wait();
  EXPECT_FALSE(IsDisplayingText(
      browser(), l10n_util::GetStringUTF8(
                     IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER)));
}

// Make sure that a same document navigation does not cause issues with the
// auto-reload timer.  Note that this test was added due to this case causing
// a crash.  On regression, this test may hang due to a crashed renderer.
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, IgnoresSameDocumentNavigation) {
  GURL test_url("http://error.page.auto.reload");
  InstallInterceptor(test_url, 2);

  // Wait for the error page and first autoreload, which happens immediately.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);

  EXPECT_EQ(2, interceptor()->failures());
  EXPECT_EQ(2, interceptor()->requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents, 1);
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.location='#';"));
  // The same page navigation counts as a navigation as far as the
  // TestNavigationObserver is concerned.
  observer.Wait();

  // No new requests should have been issued.
  EXPECT_EQ(2, interceptor()->failures());
  EXPECT_EQ(2, interceptor()->requests());

  // Wait for the second auto reload, which succeeds.
  content::TestNavigationObserver observer2(web_contents, 1);
  observer2.Wait();

  EXPECT_EQ(2, interceptor()->failures());
  EXPECT_EQ(3, interceptor()->requests());
}

// Interceptor that fails all requests with net::ERR_ADDRESS_UNREACHABLE.
class AddressUnreachableInterceptor : public net::URLRequestInterceptor {
 public:
  AddressUnreachableInterceptor() {}
  ~AddressUnreachableInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
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
  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ErrorPageNavigationCorrectionsFailTest::AddFilters));
  }

  void TearDownOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ErrorPageNavigationCorrectionsFailTest::RemoveFilters));
  }

 private:
  // Adds a filter that causes all correction service requests to fail with
  // ERR_ADDRESS_UNREACHABLE.
  //
  // Also adds the net::URLRequestFailedJob filter.
  static void AddFilters() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    URLRequestFailedJob::AddUrlHandler();

    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        google_util::LinkDoctorBaseURL(),
        std::unique_ptr<net::URLRequestInterceptor>(
            new AddressUnreachableInterceptor()));
  }

  static void RemoveFilters() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  // Diagnostics button should be displayed, if available on this platform.
  EXPECT_EQ(PlatformSupportsDiagnosticsTool(),
            IsDisplayingDiagnosticsLink(browser()));
}

// Checks that when an error occurs and a corrections fail to load, the stale
// cache status of the page is correctly transferred, and we can load the
// stale copy from the javascript.  Most logic copied from StaleCacheStatus
// above.
IN_PROC_BROWSER_TEST_F(ErrorPageNavigationCorrectionsFailTest,
                       StaleCacheStatusFailedCorrections) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Load cache with entry with "nocache" set, to create stale
  // cache.
  GURL test_url(embedded_test_server()->GetURL("/nocache.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      browser()->profile()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&InterceptNetworkTransactions,
                     base::RetainedRef(url_request_context_getter),
                     net::ERR_CONNECTION_FAILED));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);
  EXPECT_TRUE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
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
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
}

class ErrorPageOfflineTest : public ErrorPageTest {
 protected:

  void SetUpInProcessBrowserTestFixture() override {
#if defined(OS_CHROMEOS)
    if (enroll_) {
      // Set up fake install attributes.
      std::unique_ptr<chromeos::StubInstallAttributes> attributes =
          base::MakeUnique<chromeos::StubInstallAttributes>();
      attributes->SetCloudManaged("example.com", "fake-id");
      policy::BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
          attributes.release());
    }
#endif

    // Sets up a mock policy provider for user and device policies.
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::PolicyMap policy_map;
#if defined(OS_CHROMEOS)
    if (enroll_)
      SetEnterpriseUsersDefaults(&policy_map);
#endif
    if (set_allow_dinosaur_easter_egg_) {
      policy_map.Set(
          policy::key::kAllowDinosaurEasterEgg, policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
          base::MakeUnique<base::Value>(value_of_allow_dinosaur_easter_egg_),
          nullptr);
    }
    policy_provider_.UpdateChromePolicy(policy_map);

#if defined(OS_CHROMEOS)
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
#else
    policy::ProfilePolicyConnectorFactory::GetInstance()
        ->PushProviderForTesting(&policy_provider_);
#endif

    ErrorPageTest::SetUpInProcessBrowserTestFixture();
  }

  std::string NavigateToPageAndReadText() {
#if defined(OS_CHROMEOS)
      // Check enterprise enrollment
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()
        ->browser_policy_connector_chromeos();
    EXPECT_EQ(enroll_, connector->IsEnterpriseManaged());
#endif

    ui_test_utils::NavigateToURL(
        browser(),
        URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    std::string command = base::StringPrintf(
        "var hasText = document.querySelector('.snackbar');"
        "domAutomationController.send(hasText ? hasText.innerText : '');");

    std::string result;
    EXPECT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, command, &result));

    return result;
  }

  // Whether to set AllowDinosaurEasterEgg policy
  bool set_allow_dinosaur_easter_egg_ = false;

  // The value of AllowDinosaurEasterEgg policy we want to set
  bool value_of_allow_dinosaur_easter_egg_;

#if defined(OS_CHROMEOS)
  // Whether to enroll this CrOS device
  bool enroll_ = true;
#endif

  // Mock policy provider for both user and device policies.
  policy::MockConfigurationPolicyProvider policy_provider_;
};

class ErrorPageOfflineTestWithAllowDinosaurTrue : public ErrorPageOfflineTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    set_allow_dinosaur_easter_egg_ = true;
    value_of_allow_dinosaur_easter_egg_ = true;
    ErrorPageOfflineTest::SetUpInProcessBrowserTestFixture();
  }
};

class ErrorPageOfflineTestWithAllowDinosaurFalse : public ErrorPageOfflineTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    set_allow_dinosaur_easter_egg_ = true;
    value_of_allow_dinosaur_easter_egg_ = false;
    ErrorPageOfflineTest::SetUpInProcessBrowserTestFixture();
  }
};

#if defined(OS_CHROMEOS)
class ErrorPageOfflineTestUnEnrolledChromeOS : public ErrorPageOfflineTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    set_allow_dinosaur_easter_egg_ = false;
    enroll_ = false;
    ErrorPageOfflineTest::SetUpInProcessBrowserTestFixture();
  }
};
#endif

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestWithAllowDinosaurTrue,
                       CheckEasterEggIsAllowed) {
  std::string result = NavigateToPageAndReadText();
  EXPECT_EQ("", result);
}

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestWithAllowDinosaurFalse,
                       CheckEasterEggIsDisabled) {
  std::string result = NavigateToPageAndReadText();
  std::string disabled_text =
      l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED);
  EXPECT_EQ(disabled_text, result);
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTest, CheckEasterEggIsDisabled) {
  std::string result = NavigateToPageAndReadText();
  std::string disabled_text =
      l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED);
  EXPECT_EQ(disabled_text, result);
}
#else
IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTest, CheckEasterEggIsAllowed) {
  std::string result = NavigateToPageAndReadText();
  EXPECT_EQ("", result);
}
#endif

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestUnEnrolledChromeOS,
                       CheckEasterEggIsAllowed) {
  std::string result = NavigateToPageAndReadText();
  std::string disabled_text =
      l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED);
  EXPECT_EQ("", result);
}
#endif

// A test fixture that simulates failing requests for an IDN domain name.
class ErrorPageForIDNTest : public InProcessBrowserTest {
 public:
  // Target hostname in different forms.
  static const char kHostname[];
  static const char kHostnameJSUnicode[];

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Clear AcceptLanguages to force punycode decoding.
    browser()->profile()->GetPrefs()->SetString(prefs::kAcceptLanguages,
                                                std::string());
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(&ErrorPageForIDNTest::AddFilters));
  }

  void TearDownOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ErrorPageForIDNTest::RemoveFilters));
  }

 private:
  static void AddFilters() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    URLRequestFailedJob::AddUrlHandlerForHostname(kHostname);
  }

  static void RemoveFilters() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
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
  ui_test_utils::NavigateToURL(
      browser(),
      URLRequestFailedJob::GetMockHttpUrlForHostname(net::ERR_UNSAFE_PORT,
                                                     kHostname));
  EXPECT_TRUE(IsDisplayingText(browser(), kHostnameJSUnicode));
}

// Make sure HTTP/0.9 is disabled on non-default ports by default.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Http09WeirdPort) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echo-raw?spam"));
  ExpectDisplayingLocalErrorPage(browser(), net::ERR_INVALID_HTTP_RESPONSE);
}

// Test that redirects to invalid URLs show an error. See
// https://crbug.com/462272.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, RedirectToInvalidURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/server-redirect?https://:");
  ui_test_utils::NavigateToURL(browser(), url);
  ExpectDisplayingLocalErrorPage(browser(), net::ERR_INVALID_REDIRECT);
  // The error page should commit before the redirect, not after.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

class ErrorPageWithHttp09OnNonDefaultPortsTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PolicyMap values;
    values.Set(policy::key::kHttp09OnNonDefaultPortsEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(true)), nullptr);
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
  }

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
};

// Make sure HTTP/0.9 works on non-default ports when enabled by policy.
IN_PROC_BROWSER_TEST_F(ErrorPageWithHttp09OnNonDefaultPortsTest,
                       Http09WeirdPortEnabled) {
  const char kHttp09Response[] = "JumboShrimp";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(std::string("/echo-raw?") +
                                                kHttp09Response));
  EXPECT_TRUE(IsDisplayingText(browser(), kHttp09Response));
}

}  // namespace
