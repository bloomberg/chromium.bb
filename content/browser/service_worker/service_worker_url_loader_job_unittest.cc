// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_loader_job.h"

#include "base/run_loop.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/fetch/fetch_api_request.mojom.h"

namespace content {

namespace {

void ReceiveStartLoaderCallback(StartLoaderCallback* out_callback,
                                StartLoaderCallback callback) {
  *out_callback = std::move(callback);
}

}  // namespace

// ServiceWorkerURLLoaderJobTest is for testing the handling of requests
// by a service worker via ServiceWorkerURLLoaderJob.
//
// Of course, no actual service worker runs in the unit test, it is simulated
// via EmbeddedWorkerTestHelper receiving IPC messages from the browser and
// responding as if a service worker is running in the renderer.
//
// ServiceWorkerURLLoaderJobTest is also a ServiceWorkerURLLoaderJob::Delegate.
// In production code, ServiceWorkerControlleeRequestHandler is the Delegate
// (for non-"foreign fetch" request interceptions). So this class also basically
// mocks that part of ServiceWorkerControlleeRequestHandler.
class ServiceWorkerURLLoaderJobTest
    : public testing::Test,
      public ServiceWorkerURLLoaderJob::Delegate {
 public:
  ServiceWorkerURLLoaderJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        helper_(base::MakeUnique<EmbeddedWorkerTestHelper>(base::FilePath())) {}
  ~ServiceWorkerURLLoaderJobTest() override = default;

  void SetUp() override {
    // Create an active service worker.
    storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    registration_ = new ServiceWorkerRegistration(
        ServiceWorkerRegistrationOptions(GURL("https://example.com/")),
        storage()->NewRegistrationId(), helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), GURL("https://example.com/service_worker.js"),
        storage()->NewVersionId(), helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(ServiceWorkerDatabase::ResourceRecord(
        storage()->NewResourceId(), version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_);

    // Make the registration findable via storage functions.
    registration_->set_last_update_check(base::Time::Now());
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    storage()->StoreRegistration(registration_.get(), version_.get(),
                                 CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    // TODO(falken): Also setup blob_context_ for tests that will need it.
  }

  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  base::WeakPtr<storage::BlobStorageContext> GetBlobStorageContext() {
    return blob_context_.AsWeakPtr();
  }

 protected:
  // ServiceWorkerURLLoaderJob::Delegate --------------------------------------
  void OnPrepareToRestart() override {}

  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    return version_.get();
  }

  bool RequestStillValid(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    return true;
  }

  void MainResourceLoadFailed() override {}
  // --------------------------------------------------------------------------

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  storage::BlobStorageContext blob_context_;
};

TEST_F(ServiceWorkerURLLoaderJobTest, Basic) {
  ResourceRequest request;
  request.url = GURL("https://www.example.com/");
  request.method = "GET";

  // Start a ServiceWorkerURLLoaderJob. It should return a StartLoaderCallback.
  StartLoaderCallback callback;
  auto job = base::MakeUnique<ServiceWorkerURLLoaderJob>(
      base::BindOnce(&ReceiveStartLoaderCallback, &callback), this, request,
      GetBlobStorageContext());
  job->ForwardToServiceWorker();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.is_null());

  // Start the loader. It will load the URL.
  TestURLLoaderClient client;
  mojom::URLLoaderPtr loader;
  std::move(callback).Run(mojo::MakeRequest(&loader),
                          client.CreateInterfacePtr());
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);

  // The URL should have loaded successfully via service worker.
  const ResourceResponseHead& info = client.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  EXPECT_TRUE(info.was_fetched_via_service_worker);
  EXPECT_FALSE(info.was_fallback_required_by_service_worker);
  EXPECT_TRUE(info.url_list_via_service_worker.empty());
  EXPECT_EQ(blink::mojom::FetchResponseType::kDefault,
            info.response_type_via_service_worker);
  // TODO(falken): start and ready time should be set.
  EXPECT_TRUE(info.service_worker_start_time.is_null());
  EXPECT_TRUE(info.service_worker_ready_time.is_null());
  EXPECT_FALSE(info.is_in_cache_storage);
  EXPECT_EQ(std::string(), info.cache_storage_cache_name);
}

}  // namespace content
