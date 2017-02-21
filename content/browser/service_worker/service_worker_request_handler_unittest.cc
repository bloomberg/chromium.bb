// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <utility>

#include "base/run_loop.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockProviderId = 1;

}

class ServiceWorkerRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    // An empty host.
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(
            helper_->mock_render_process_id(), kMockProviderId,
            true /* is_parent_frame_secure */, context()->AsWeakPtr());
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));
  }

  void TearDown() override {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const std::string& url,
                                                 const std::string& method) {
    std::unique_ptr<net::URLRequest> request =
        url_request_context_.CreateRequest(GURL(url), net::DEFAULT_PRIORITY,
                                           &url_request_delegate_);
    request->set_method(method);
    return request;
  }

  void InitializeHandler(net::URLRequest* request,
                         bool skip_service_worker,
                         ResourceType resource_type) {
    ServiceWorkerRequestHandler::InitializeHandler(
        request, context_wrapper(), &blob_storage_context_,
        helper_->mock_render_process_id(), kMockProviderId, skip_service_worker,
        FETCH_REQUEST_MODE_NO_CORS, FETCH_CREDENTIALS_MODE_OMIT,
        FetchRedirectMode::FOLLOW_MODE, resource_type,
        REQUEST_CONTEXT_TYPE_HYPERLINK, REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
        nullptr);
  }

  static ServiceWorkerRequestHandler* GetHandler(net::URLRequest* request) {
    return ServiceWorkerRequestHandler::GetHandler(request);
  }

  std::unique_ptr<net::URLRequestJob> MaybeCreateJob(net::URLRequest* request) {
    return std::unique_ptr<net::URLRequestJob>(
        GetHandler(request)->MaybeCreateJob(
            request, url_request_context_.network_delegate(),
            context_wrapper()->resource_context()));
  }

  void InitializeHandlerSimpleTest(const std::string& url,
                                   const std::string& method,
                                   bool skip_service_worker,
                                   ResourceType resource_type) {
    std::unique_ptr<net::URLRequest> request = CreateRequest(url, method);
    InitializeHandler(request.get(), skip_service_worker, resource_type);
    ASSERT_TRUE(GetHandler(request.get()));
    MaybeCreateJob(request.get());
    EXPECT_EQ(url, provider_host_->document_url().spec());
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  storage::BlobStorageContext blob_storage_context_;
};

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_FTP) {
  std::unique_ptr<net::URLRequest> request =
      CreateRequest("ftp://host/scope/doc", "GET");
  InitializeHandler(request.get(), false, RESOURCE_TYPE_MAIN_FRAME);
  // Cannot initialize a handler for non-secure origins.
  EXPECT_FALSE(GetHandler(request.get()));
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTP_MAIN_FRAME) {
  // HTTP should have the handler because the response is possible to be a
  // redirect to HTTPS.
  InitializeHandlerSimpleTest("http://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_MAIN_FRAME) {
  InitializeHandlerSimpleTest("https://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTP_SUB_FRAME) {
  // HTTP should have the handler because the response is possible to be a
  // redirect to HTTPS.
  InitializeHandlerSimpleTest("http://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_SUB_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_SUB_FRAME) {
  InitializeHandlerSimpleTest("https://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_SUB_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_OPTIONS) {
  // OPTIONS is also supported. See crbug.com/434660.
  InitializeHandlerSimpleTest("https://host/scope/doc", "OPTIONS", false,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_SKIP) {
  InitializeHandlerSimpleTest("https://host/scope/doc", "GET", true,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_IMAGE) {
  // Check provider host's URL after initializing a handler for an image.
  provider_host_->SetDocumentUrl(GURL("https://host/scope/doc"));
  std::unique_ptr<net::URLRequest> request =
      CreateRequest("https://host/scope/image", "GET");
  InitializeHandler(request.get(), true, RESOURCE_TYPE_IMAGE);
  ASSERT_FALSE(GetHandler(request.get()));
  EXPECT_EQ(GURL("https://host/scope/doc"), provider_host_->document_url());
}

}  // namespace content
