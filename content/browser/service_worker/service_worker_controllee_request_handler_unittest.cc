// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockRenderProcessId = 1224;
int kMockProviderId = 1;

void EmptyCallback() {}

}

class ServiceWorkerControlleeRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerControlleeRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(kMockRenderProcessId));

    // A new unstored registration/version.
    scope_ = GURL("http://host/scope/");
    script_url_ = GURL("http://host/script.js");
    registration_ = new ServiceWorkerRegistration(
        scope_, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, 1L, context()->AsWeakPtr());

    // An empty host.
    scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
        kMockRenderProcessId, MSG_ROUTING_NONE, kMockProviderId,
        SERVICE_WORKER_PROVIDER_FOR_CONTROLLEE,
        context()->AsWeakPtr(), NULL));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(host.Pass());

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    version_ = NULL;
    registration_ = NULL;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  MockResourceContext mock_resource_context_;
  GURL scope_;
  GURL script_url_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() {}
  bool AllowServiceWorker(const GURL& scope,
                          const GURL& first_party,
                          content::ResourceContext* context) override {
    return false;
  }
};

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DisallowServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  // Store an activated worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_.get());
  context()->storage()->StoreRegistration(
      registration_.get(),
      version_.get(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  const GURL kDocUrl("http://host/scope/doc");
  scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kDocUrl, net::DEFAULT_PRIORITY, &url_request_delegate_, NULL);
  scoped_ptr<ServiceWorkerControlleeRequestHandler> handler(
      new ServiceWorkerControlleeRequestHandler(
          context()->AsWeakPtr(),
          provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          FETCH_REQUEST_MODE_NO_CORS,
          FETCH_CREDENTIALS_MODE_OMIT,
          RESOURCE_TYPE_MAIN_FRAME,
          REQUEST_CONTEXT_TYPE_HYPERLINK,
          REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
          scoped_refptr<ResourceRequestBody>()));
  scoped_refptr<net::URLRequestJob> job =
      handler->MaybeCreateJob(request.get(), NULL, &mock_resource_context_);
  ServiceWorkerURLRequestJob* sw_job =
      static_cast<ServiceWorkerURLRequestJob*>(job.get());

  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());
  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_TRUE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, ActivateWaitingVersion) {
  // Store a registration that is installed but not activated yet.
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration_->SetWaitingVersion(version_.get());
  context()->storage()->StoreRegistration(
      registration_.get(),
      version_.get(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  const GURL kDocUrl("http://host/scope/doc");
  scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kDocUrl,
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      NULL);
  scoped_ptr<ServiceWorkerControlleeRequestHandler> handler(
      new ServiceWorkerControlleeRequestHandler(
          context()->AsWeakPtr(),
          provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          FETCH_REQUEST_MODE_NO_CORS,
          FETCH_CREDENTIALS_MODE_OMIT,
          RESOURCE_TYPE_MAIN_FRAME,
          REQUEST_CONTEXT_TYPE_HYPERLINK,
          REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
          scoped_refptr<ResourceRequestBody>()));
  scoped_refptr<net::URLRequestJob> job =
      handler->MaybeCreateJob(request.get(), NULL, &mock_resource_context_);
  ServiceWorkerURLRequestJob* sw_job =
      static_cast<ServiceWorkerURLRequestJob*>(job.get());

  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            version_->status());
  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_TRUE(sw_job->ShouldForwardToServiceWorker());
  EXPECT_TRUE(version_->HasControllee());

  // Navigations should trigger an update too.
  handler.reset(NULL);
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test to not regress crbug/414118.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, DeletedProviderHost) {
  // Store a registration so the call to FindRegistrationForDocument will read
  // from the database.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_.get());
  context()->storage()->StoreRegistration(
      registration_.get(),
      version_.get(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  version_ = NULL;
  registration_ = NULL;

  // Conduct a main resource load.
  const GURL kDocUrl("http://host/scope/doc");
  scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kDocUrl,
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      NULL);
  scoped_ptr<ServiceWorkerControlleeRequestHandler> handler(
      new ServiceWorkerControlleeRequestHandler(
          context()->AsWeakPtr(),
          provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          FETCH_REQUEST_MODE_NO_CORS,
          FETCH_CREDENTIALS_MODE_OMIT,
          RESOURCE_TYPE_MAIN_FRAME,
          REQUEST_CONTEXT_TYPE_HYPERLINK,
          REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
          scoped_refptr<ResourceRequestBody>()));
  scoped_refptr<net::URLRequestJob> job =
      handler->MaybeCreateJob(request.get(), NULL, &mock_resource_context_);
  ServiceWorkerURLRequestJob* sw_job =
      static_cast<ServiceWorkerURLRequestJob*>(job.get());

  EXPECT_FALSE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());

  // Shouldn't crash if the ProviderHost is deleted prior to completion of
  // the database lookup.
  context()->RemoveProviderHost(kMockRenderProcessId, kMockProviderId);
  EXPECT_FALSE(provider_host_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sw_job->ShouldFallbackToNetwork());
  EXPECT_FALSE(sw_job->ShouldForwardToServiceWorker());
}

}  // namespace content
