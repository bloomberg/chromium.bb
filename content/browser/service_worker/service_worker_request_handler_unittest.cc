// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include "base/run_loop.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/resource_request_body.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockRenderProcessId = 1224;
int kMockProviderId = 1;

void EmptyCallback() {
}

}

class ServiceWorkerRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(kMockRenderProcessId));

    // A new unstored registration/version.
    registration_ = new ServiceWorkerRegistration(
        GURL("http://host/scope/"), 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(),
                                        GURL("http://host/script.js"),
                                        1L,
                                        context()->AsWeakPtr());

    // An empty host.
    scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
        kMockRenderProcessId, kMockProviderId, context()->AsWeakPtr(), NULL));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(host.Pass());

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();

    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_.get());
    context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    provider_host_->AssociateRegistration(registration_.get());
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    version_ = NULL;
    registration_ = NULL;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

  bool InitializeHandlerCheck(const std::string& url,
                              const std::string& method,
                              bool skip_service_worker,
                              ResourceType resource_type) {
    const GURL kDocUrl(url);
    scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
        kDocUrl, net::DEFAULT_PRIORITY, &url_request_delegate_, NULL);
    request->set_method(method);
    ServiceWorkerRequestHandler::InitializeHandler(request.get(),
                                                   context_wrapper(),
                                                   &blob_storage_context_,
                                                   kMockRenderProcessId,
                                                   kMockProviderId,
                                                   skip_service_worker,
                                                   resource_type,
                                                   NULL);
    return ServiceWorkerRequestHandler::GetHandler(request.get()) != NULL;
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  storage::BlobStorageContext blob_storage_context_;
};

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler) {
  EXPECT_TRUE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", false, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_TRUE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", false, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_FALSE(InitializeHandlerCheck(
      "ftp://host/scope/doc", "GET", false, RESOURCE_TYPE_MAIN_FRAME));

  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "OPTIONS", false, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "OPTIONS", false, RESOURCE_TYPE_MAIN_FRAME));

  provider_host_->SetDocumentUrl(GURL(""));
  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", true, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_STREQ("http://host/scope/doc",
               provider_host_->document_url().spec().c_str());
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", true, RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_STREQ("https://host/scope/doc",
               provider_host_->document_url().spec().c_str());

  provider_host_->SetDocumentUrl(GURL(""));
  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", true, RESOURCE_TYPE_SUB_FRAME));
  EXPECT_STREQ("http://host/scope/doc",
               provider_host_->document_url().spec().c_str());
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", true, RESOURCE_TYPE_SUB_FRAME));
  EXPECT_STREQ("https://host/scope/doc",
               provider_host_->document_url().spec().c_str());

  provider_host_->SetDocumentUrl(GURL(""));
  EXPECT_FALSE(InitializeHandlerCheck(
      "http://host/scope/doc", "GET", true, RESOURCE_TYPE_IMAGE));
  EXPECT_STREQ("", provider_host_->document_url().spec().c_str());
  EXPECT_FALSE(InitializeHandlerCheck(
      "https://host/scope/doc", "GET", true, RESOURCE_TYPE_IMAGE));
  EXPECT_STREQ("", provider_host_->document_url().spec().c_str());
}

}  // namespace content
