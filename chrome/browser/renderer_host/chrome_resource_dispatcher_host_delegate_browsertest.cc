// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request.h"

using content::ResourceType;

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
  TestDispatcherHostDelegate() {}
  ~TestDispatcherHostDelegate() override {}

  void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      ResourceType resource_type,
      ScopedVector<content::ResourceThrottle>* throttles) override {
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
                           content::ResourceResponse* response) override {
    ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
        redirect_url,
        request,
        resource_context,
        response);
    request_headers_.MergeFrom(request->extra_request_headers());
  }

  net::HttpRequestHeaders request_headers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDispatcherHostDelegate);
};

}  // namespace

class ChromeResourceDispatcherHostDelegateBrowserTest :
    public InProcessBrowserTest {
 public:
  ChromeResourceDispatcherHostDelegateBrowserTest() {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
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
  }

  void TearDownOnMainThread() override {
    content::ResourceDispatcherHost::Get()->SetDelegate(NULL);
    dispatcher_host_delegate_.reset();
  }

 protected:
  // The fake URL for DMServer we are using.
  GURL dm_url_;
  std::unique_ptr<TestDispatcherHostDelegate> dispatcher_host_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegateBrowserTest);
};


IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       NoPolicyHeader) {
  // When fetching non-DMServer URLs, we should not add a policy header to the
  // request.
  DCHECK(!embedded_test_server()->base_url().spec().empty());
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  ASSERT_FALSE(dispatcher_host_delegate_->request_headers_.HasHeader(
      policy::kChromePolicyHeader));
}

IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       PolicyHeader) {
  // When fetching a DMServer URL, we should add a policy header to the
  // request.
  ui_test_utils::NavigateToURL(browser(), dm_url_);
  std::string value;
  ASSERT_TRUE(dispatcher_host_delegate_->request_headers_.GetHeader(
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
  ASSERT_TRUE(dispatcher_host_delegate_->request_headers_.GetHeader(
      policy::kChromePolicyHeader, &value));
  ASSERT_EQ(kTestPolicyHeader, value);
}
