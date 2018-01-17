// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_browsertest.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using content::ResourceType;
using testing::HasSubstr;
using testing::Not;

namespace {

static const char kTestPolicyHeader[] = "test_header";
static const char kServerRedirectUrl[] = "/server-redirect";

std::unique_ptr<net::test_server::HttpResponse> HandleTestRequest(
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, kServerRedirectUrl,
                       base::CompareCase::SENSITIVE)) {
    // Extract the target URL and redirect there.
    size_t query_string_pos = request.relative_url.find('?');
    std::string redirect_target =
        request.relative_url.substr(query_string_pos + 1);

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_target);
    return std::move(http_response);
  } else {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("Success");
    return std::move(http_response);
  }
}

class TestDispatcherHostDelegate : public ChromeResourceDispatcherHostDelegate {
 public:
  TestDispatcherHostDelegate() : should_add_data_reduction_proxy_data_(false) {}
  ~TestDispatcherHostDelegate() override {}

  // ResourceDispatcherHostDelegate implementation:
  void RequestBeginning(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        content::AppCacheService* appcache_service,
                        ResourceType resource_type,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override {
    ChromeResourceDispatcherHostDelegate::RequestBeginning(
        request,
        resource_context,
        appcache_service,
        resource_type,
        throttles);
    request_headers_.MergeFrom(request->extra_request_headers());
  }

  void OnRequestRedirected(const GURL& redirect_url,
                           net::URLRequest* request,
                           content::ResourceContext* resource_context,
                           network::ResourceResponse* response) override {
    ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
        redirect_url,
        request,
        resource_context,
        response);
    request_headers_.MergeFrom(request->extra_request_headers());
  }

  content::NavigationData* GetNavigationData(
      net::URLRequest* request) const override {
    if (request && should_add_data_reduction_proxy_data_) {
      data_reduction_proxy::DataReductionProxyData* data =
          data_reduction_proxy::DataReductionProxyData::
              GetDataAndCreateIfNecessary(request);
      data->set_used_data_reduction_proxy(true);
    }
    return ChromeResourceDispatcherHostDelegate::GetNavigationData(request);
  }

  // ChromeResourceDispatcherHost implementation:
  void AppendStandardResourceThrottles(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      ResourceType resource_type,
      std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles)
      override {
    ++times_stardard_throttles_added_for_url_[request->url()];
    ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
        request, resource_context, resource_type, throttles);
  }

  void set_should_add_data_reduction_proxy_data(
      bool should_add_data_reduction_proxy_data) {
    should_add_data_reduction_proxy_data_ =
        should_add_data_reduction_proxy_data;
  }

  const net::HttpRequestHeaders& request_headers() const {
    return request_headers_;
  }

  // Writes the number of times the standard set of throttles have been added
  // for requests for the speficied URL to |count|.
  void GetTimesStandardThrottlesAddedForURL(const GURL& url, int* count) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    *count = times_stardard_throttles_added_for_url_[url];
  }

 private:
  bool should_add_data_reduction_proxy_data_;
  net::HttpRequestHeaders request_headers_;

  std::map<GURL, int> times_stardard_throttles_added_for_url_;

  DISALLOW_COPY_AND_ASSIGN(TestDispatcherHostDelegate);
};

// Helper class to track DidFinishNavigation and verify that NavigationData is
// added to NavigationHandle and pause/resume execution of the test.
class DidFinishNavigationObserver : public content::WebContentsObserver {
 public:
  DidFinishNavigationObserver(content::WebContents* web_contents,
                                       bool add_data_reduction_proxy_data)
      : content::WebContentsObserver(web_contents),
        add_data_reduction_proxy_data_(add_data_reduction_proxy_data) {}
  ~DidFinishNavigationObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ChromeNavigationData* data = static_cast<ChromeNavigationData*>(
        navigation_handle->GetNavigationData());
    if (add_data_reduction_proxy_data_) {
      EXPECT_TRUE(data->GetDataReductionProxyData());
      EXPECT_TRUE(
          data->GetDataReductionProxyData()->used_data_reduction_proxy());
    } else {
      EXPECT_FALSE(data->GetDataReductionProxyData());
    }
  }

 private:
  bool add_data_reduction_proxy_data_;
  DISALLOW_COPY_AND_ASSIGN(DidFinishNavigationObserver);
};

}  // namespace

class ChromeResourceDispatcherHostDelegateBrowserTest :
    public InProcessBrowserTest {
 public:
  ChromeResourceDispatcherHostDelegateBrowserTest() {}

  void SetUpOnMainThread() override {
    // Hook navigations with our delegate.
    dispatcher_host_delegate_.reset(new TestDispatcherHostDelegate);
    content::ResourceDispatcherHost::Get()->SetDelegate(
        dispatcher_host_delegate_.get());

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&HandleTestRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
    // Tell chrome that this is our DM server.
    dm_url_ = embedded_test_server()->GetURL("/DeviceManagement");

    // At this point, the Profile is already initialized and it's too
    // late to set the DMServer URL via command line flags, so directly
    // inject it to the PolicyHeaderIOHelper.
    policy::PolicyHeaderService* policy_header_service =
        policy::PolicyHeaderServiceFactory::GetForBrowserContext(
            browser()->profile());
    std::vector<policy::PolicyHeaderIOHelper*> helpers =
        policy_header_service->GetHelpersForTest();
    for (std::vector<policy::PolicyHeaderIOHelper*>::const_iterator it =
             helpers.begin();
         it != helpers.end(); ++it) {
      (*it)->SetServerURLForTest(dm_url_.spec());
      (*it)->UpdateHeader(kTestPolicyHeader);
    }

    // Set up temp directory for downloads.
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, downloads_directory_.GetPath());
  }

  void TearDownOnMainThread() override {
    content::ResourceDispatcherHost::Get()->SetDelegate(NULL);
    dispatcher_host_delegate_.reset();
  }

  void SetShouldAddDataReductionProxyData(bool add_data) {
    dispatcher_host_delegate_->set_should_add_data_reduction_proxy_data(
        add_data);
  }

  int GetTimesStandardThrottlesAddedForURL(const GURL& url) {
    int count;
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &TestDispatcherHostDelegate::GetTimesStandardThrottlesAddedForURL,
            base::Unretained(dispatcher_host_delegate_.get()), url, &count),
        run_loop.QuitClosure());
    run_loop.Run();
    return count;
  }

 protected:
  // The fake URL for DMServer we are using.
  GURL dm_url_;
  std::unique_ptr<TestDispatcherHostDelegate> dispatcher_host_delegate_;

 private:
  // Location of the downloads directory for tests that use one.
  base::ScopedTempDir downloads_directory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegateBrowserTest);
};


IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       NoPolicyHeader) {
  // When fetching non-DMServer URLs, we should not add a policy header to the
  // request.
  DCHECK(!embedded_test_server()->base_url().spec().empty());
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  ASSERT_FALSE(dispatcher_host_delegate_->request_headers().HasHeader(
      policy::kChromePolicyHeader));
}

IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       PolicyHeader) {
  // When fetching a DMServer URL, we should add a policy header to the
  // request.
  ui_test_utils::NavigateToURL(browser(), dm_url_);
  std::string value;
  ASSERT_TRUE(dispatcher_host_delegate_->request_headers().GetHeader(
      policy::kChromePolicyHeader, &value));
  ASSERT_EQ(kTestPolicyHeader, value);
}

IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       PolicyHeaderForRedirect) {
  // Build up a URL that results in a redirect to the DMServer URL to make
  // sure the policy header is still added.
  std::string redirect_url;
  redirect_url += kServerRedirectUrl;
  redirect_url += "?";
  redirect_url += dm_url_.spec();
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
      redirect_url));
  std::string value;
  ASSERT_TRUE(dispatcher_host_delegate_->request_headers().GetHeader(
      policy::kChromePolicyHeader, &value));
  ASSERT_EQ(kTestPolicyHeader, value);
}

IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       NavigationDataProcessed) {
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  {
    DidFinishNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), false);
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/google/google.html"));
  }
  SetShouldAddDataReductionProxyData(true);
  {
    DidFinishNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), true);
    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  }
}

namespace {

// A URLRequestMockHTTPJob to that reports HTTP request headers of outgoing
// requests.
class MirrorMockURLRequestJob : public net::URLRequestMockHTTPJob {
 public:
  // Callback function on the UI thread to report a (URL, request headers)
  // pair. Indicating that the |request headers| will be sent for the |URL|.
  using ReportResponseHeadersOnUI =
      base::Callback<void(const std::string&, const std::string&)>;

  MirrorMockURLRequestJob(net::URLRequest* request,
                          net::NetworkDelegate* network_delegate,
                          const base::FilePath& file_path,
                          ReportResponseHeadersOnUI report_on_ui)
      : net::URLRequestMockHTTPJob(request, network_delegate, file_path),
        report_on_ui_(report_on_ui) {}

  void Start() override {
    // Report the observed request headers on the UI thread.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(report_on_ui_, request_->url().spec(),
                       request_->extra_request_headers().ToString()));

    URLRequestMockHTTPJob::Start();
  }

 protected:
  const ReportResponseHeadersOnUI report_on_ui_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MirrorMockURLRequestJob);
};

// A URLRequestInterceptor to inject MirrorMockURLRequestJobs.
class MirrorMockJobInterceptor : public net::URLRequestInterceptor {
 public:
  using ReportResponseHeadersOnUI =
      MirrorMockURLRequestJob::ReportResponseHeadersOnUI;

  MirrorMockJobInterceptor(const base::FilePath& root_http,
                           ReportResponseHeadersOnUI report_on_ui)
      : root_http_(root_http), report_on_ui_(report_on_ui) {}
  ~MirrorMockJobInterceptor() override = default;

  // URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new MirrorMockURLRequestJob(
        request, network_delegate, root_http_,
        report_on_ui_);
  }

  static void Register(const GURL& url,
                       const base::FilePath& root_http,
                       ReportResponseHeadersOnUI report_on_ui) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    base::FilePath file_path(root_http);
    file_path =
        file_path.AppendASCII(url.scheme() + "." + url.host() + ".html");
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        url, base::WrapUnique(
                 new MirrorMockJobInterceptor(file_path, report_on_ui)));
  }

  static void Unregister(const GURL& url) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->RemoveUrlHandler(url);
  }

 private:
  const base::FilePath root_http_;
  ReportResponseHeadersOnUI report_on_ui_;

  DISALLOW_COPY_AND_ASSIGN(MirrorMockJobInterceptor);
};

// Used in MirrorMockURLRequestJob to store HTTP request header for all received
// URLs in the given map.
void ReportRequestHeaders(std::map<std::string, std::string>* request_headers,
                          const std::string& url,
                          const std::string& headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ensure that a previous value is not overwritten.
  EXPECT_FALSE(base::ContainsKey(*request_headers, url));
  (*request_headers)[url] = headers;
}

}  // namespace

// A delegate to insert a user generated X-Chrome-Connected header
// to a specifict URL.
class HeaderTestDispatcherHostDelegate
    : public ChromeResourceDispatcherHostDelegate {
 public:
  explicit HeaderTestDispatcherHostDelegate(const GURL& watch_url)
      : watch_url_(watch_url) {}
  ~HeaderTestDispatcherHostDelegate() override {}

  void RequestBeginning(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        content::AppCacheService* appcache_service,
                        ResourceType resource_type,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override {
    ChromeResourceDispatcherHostDelegate::RequestBeginning(
        request, resource_context, appcache_service, resource_type, throttles);
    if (request->url() == watch_url_) {
      request->SetExtraRequestHeaderByName(signin::kChromeConnectedHeader,
                                           "User Data", false);
    }
  }

 private:
  const GURL watch_url_;

  DISALLOW_COPY_AND_ASSIGN(HeaderTestDispatcherHostDelegate);
};

// Sets a new one on IO thread
void SetDelegateOnIO(content::ResourceDispatcherHostDelegate* new_delegate) {
  content::ResourceDispatcherHost::Get()->SetDelegate(new_delegate);
}

// Verify the following items:
// 1- X-Chrome-Connected is appended on Google domains if account
//    consistency is enabled and access is secure.
// 2- The header is stripped in case a request is redirected from a Gooogle
//    domain to non-google domain.
// 3- The header is NOT stripped in case it is added directly by the page
//    and not because it was on a secure Google domain.
// This is a regression test for crbug.com/588492.
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       MirrorRequestHeader) {
  // Enable account consistency so that mirror actually sets the
  // X-Chrome-Connected header in requests to Google.
  signin::ScopedAccountConsistencyMirror scoped_mirror;

  browser()->profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                              "user@gmail.com");
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesUserAccountId, "account_id");

  base::FilePath root_http;
  PathService::Get(chrome::DIR_TEST_DATA, &root_http);
  root_http = root_http.AppendASCII("mirror_request_header");

  struct TestCase {
    GURL original_url;       // The URL from which the request begins.
    GURL redirected_to_url;  // The URL to which naviagtion is redirected.
    bool inject_header;  // Should X-Chrome-Connected header be injected to the
                         // original request.
    bool original_url_expects_header;       // Expectation: The header should be
                                            // visible in original URL.
    bool redirected_to_url_expects_header;  // Expectation: The header should be
                                            // visible in redirected URL.
  } all_tests[] = {
      // Neither should have the header.
      {GURL("http://www.google.com"), GURL("http://www.redirected.com"), false,
       false, false},
      // First one should have the header, but not transfered to second one.
      {GURL("https://www.google.com"), GURL("https://www.redirected.com"),
       false, true, false},
      // First one adds the header and transfers it to the second.
      {GURL("http://www.header_adder.com"),
       GURL("http://www.redirected_from_header_adder.com"), true, true, true}};

  for (const auto& test_case : all_tests) {
    SCOPED_TRACE(test_case.original_url);

    // The HTTP Request headers (i.e. the ones that are managed on the
    // URLRequest layer, not on the URLRequestJob layer) sent from the browser
    // are collected in this map. The keys are URLs the values the request
    // headers.
    std::map<std::string, std::string> request_headers;
    MirrorMockURLRequestJob::ReportResponseHeadersOnUI report_request_headers =
        base::Bind(&ReportRequestHeaders, &request_headers);

    // If test case requires adding header for the first url,
    // change the delegate.
    std::unique_ptr<HeaderTestDispatcherHostDelegate> dispatcher_host_delegate;
    if (test_case.inject_header) {
      dispatcher_host_delegate =
          base::MakeUnique<HeaderTestDispatcherHostDelegate>(
              test_case.original_url);
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&SetDelegateOnIO, dispatcher_host_delegate.get()));
    }

    // Set up mockup interceptors.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&MirrorMockJobInterceptor::Register,
                       test_case.original_url, root_http,
                       report_request_headers));
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&MirrorMockJobInterceptor::Register,
                       test_case.redirected_to_url, root_http,
                       report_request_headers));

    // Navigate to first url.
    ui_test_utils::NavigateToURL(browser(), test_case.original_url);

    // Cleanup before verifying the observed headers.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&MirrorMockJobInterceptor::Unregister,
                       test_case.original_url));
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&MirrorMockJobInterceptor::Unregister,
                       test_case.redirected_to_url));

    // If delegate is changed, remove it.
    if (test_case.inject_header) {
      base::RunLoop run_loop;
      content::BrowserThread::PostTaskAndReply(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&SetDelegateOnIO, nullptr), run_loop.QuitClosure());
      run_loop.Run();
    }

    // Ensure that the response headers have been reported to the UI thread
    // and unregistration has been processed on the IO thread.
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(content::BrowserThread::IO,
                                             FROM_HERE,
                                             // Flush IO thread...
                                             base::BindOnce(&base::DoNothing),
                                             // ... and UI thread.
                                             run_loop.QuitClosure());
    run_loop.Run();

    // Check if header exists and X-Chrome-Connected is correctly provided.
    ASSERT_EQ(1u, request_headers.count(test_case.original_url.spec()));
    if (test_case.original_url_expects_header) {
      EXPECT_THAT(request_headers[test_case.original_url.spec()],
                  HasSubstr(signin::kChromeConnectedHeader));
    } else {
      EXPECT_THAT(request_headers[test_case.original_url.spec()],
                  Not(HasSubstr(signin::kChromeConnectedHeader)));
    }

    ASSERT_EQ(1u, request_headers.count(test_case.redirected_to_url.spec()));
    if (test_case.redirected_to_url_expects_header) {
      EXPECT_THAT(request_headers[test_case.redirected_to_url.spec()],
                  HasSubstr(signin::kChromeConnectedHeader));
    } else {
      EXPECT_THAT(request_headers[test_case.redirected_to_url.spec()],
                  Not(HasSubstr(signin::kChromeConnectedHeader)));
    }
  }
}

// Check that exactly one set of throttles is added to smaller downloads, which
// have their mime type determined only after the response is completely
// received.
// See https://crbug.com/640545
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       ThrottlesAddedExactlyOnceToTinySniffedDownloads) {
  GURL url = embedded_test_server()->GetURL("/downloads/tiny_binary.bin");
  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1);
  download_observer.StartObserving();
  ui_test_utils::NavigateToURL(browser(), url);
  download_observer.WaitForFinished();
  EXPECT_EQ(1, GetTimesStandardThrottlesAddedForURL(url));
}

// Check that exactly one set of throttles is added to larger downloads, which
// have their mime type determined before the end of the response is reported.
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       ThrottlesAddedExactlyOnceToLargeSniffedDownloads) {
  GURL url = embedded_test_server()->GetURL("/downloads/thisdayinhistory.xls");
  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1);
  download_observer.StartObserving();
  ui_test_utils::NavigateToURL(browser(), url);
  download_observer.WaitForFinished();
  EXPECT_EQ(1, GetTimesStandardThrottlesAddedForURL(url));
}

// Check that exactly one set of throttles is added to downloads started by an
// <a download> click.
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       ThrottlesAddedExactlyOnceToADownloads) {
  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1);
  download_observer.StartObserving();
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/download-anchor-attrib.html"));
  download_observer.WaitForFinished();
  EXPECT_EQ(1,
            GetTimesStandardThrottlesAddedForURL(
                embedded_test_server()->GetURL("/anchor_download_test.png")));
}
