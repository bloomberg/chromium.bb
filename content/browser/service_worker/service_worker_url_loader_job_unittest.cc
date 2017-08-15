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
#include "mojo/common/data_pipe_utils.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void ReceiveStartLoaderCallback(StartLoaderCallback* out_callback,
                                StartLoaderCallback callback) {
  *out_callback = std::move(callback);
}

}  // namespace

// Helper simulates a service worker handling fetch events. The response can be
// customized via RespondWith* functions.
class Helper : public EmbeddedWorkerTestHelper {
 public:
  Helper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~Helper() override = default;

  // Tells this helper to respond to fetch events with the specified blob.
  void RespondWithBlob(const std::string blob_uuid, uint64_t blob_size) {
    response_mode_ = ResponseMode::kBlob;
    blob_uuid_ = blob_uuid;
    blob_size_ = blob_size;
  }

 protected:
  void OnFetchEvent(
      int embedded_worker_id,
      int fetch_event_id,
      const ServiceWorkerFetchRequest& request,
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      FetchCallback finish_callback) override {
    switch (response_mode_) {
      case ResponseMode::kDefault:
        EmbeddedWorkerTestHelper::OnFetchEvent(
            embedded_worker_id, fetch_event_id, request,
            std::move(preload_handle), std::move(response_callback),
            std::move(finish_callback));
        return;
      case ResponseMode::kBlob:
        response_callback->OnResponse(
            ServiceWorkerResponse(
                base::MakeUnique<std::vector<GURL>>(), 200, "OK",
                network::mojom::FetchResponseType::kDefault,
                base::MakeUnique<ServiceWorkerHeaderMap>(), blob_uuid_,
                blob_size_, nullptr /* blob */,
                blink::kWebServiceWorkerResponseErrorUnknown, base::Time(),
                false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                base::MakeUnique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            base::Time::Now());
        std::move(finish_callback).Run(SERVICE_WORKER_OK, base::Time::Now());
        return;
    }
    NOTREACHED();
  }

 private:
  enum class ResponseMode { kDefault, kBlob };
  ResponseMode response_mode_ = ResponseMode::kDefault;

  // For ResponseMode::kBlob.
  std::string blob_uuid_;
  uint64_t blob_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Helper);
};

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
        helper_(base::MakeUnique<Helper>()) {}
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
  }

  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  base::WeakPtr<storage::BlobStorageContext> GetBlobStorageContext() {
    return blob_context_.AsWeakPtr();
  }

  // Performs a request. When this returns, |client_| will have information
  // about the response.
  void TestRequest() {
    ResourceRequest request;
    request.url = GURL("https://www.example.com/");
    request.method = "GET";

    // Start a ServiceWorkerURLLoaderJob. It should return a
    // StartLoaderCallback.
    StartLoaderCallback callback;
    auto job = base::MakeUnique<ServiceWorkerURLLoaderJob>(
        base::BindOnce(&ReceiveStartLoaderCallback, &callback), this, request,
        GetBlobStorageContext());
    job->ForwardToServiceWorker();
    base::RunLoop().RunUntilIdle();
    // TODO(falken): When fallback to network tests are added,
    // callback can be null. In that case this function should return cleanly
    // and somehow tell the caller we're falling back to network.
    EXPECT_FALSE(callback.is_null());

    // Start the loader. It will load |request.url|.
    mojom::URLLoaderPtr loader;
    std::move(callback).Run(mojo::MakeRequest(&loader),
                            client_.CreateInterfacePtr());
    client_.RunUntilComplete();
  }

  void ExpectFetchedViaServiceWorker(const ResourceResponseHead& info) {
    EXPECT_TRUE(info.was_fetched_via_service_worker);
    EXPECT_FALSE(info.was_fallback_required_by_service_worker);
    EXPECT_TRUE(info.url_list_via_service_worker.empty());
    EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
              info.response_type_via_service_worker);
    // TODO(falken): start and ready time should be set.
    EXPECT_TRUE(info.service_worker_start_time.is_null());
    EXPECT_TRUE(info.service_worker_ready_time.is_null());
    EXPECT_FALSE(info.is_in_cache_storage);
    EXPECT_EQ(std::string(), info.cache_storage_cache_name);
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
  std::unique_ptr<Helper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  storage::BlobStorageContext blob_context_;
  TestURLLoaderClient client_;
};

TEST_F(ServiceWorkerURLLoaderJobTest, Basic) {
  TestRequest();
  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);
}

TEST_F(ServiceWorkerURLLoaderJobTest, BlobResponse) {
  // Construct the blob to respond with.
  const std::string kResponseBody = "Here is sample text for the blob.";
  auto blob_data = base::MakeUnique<storage::BlobDataBuilder>("blob-id:myblob");
  blob_data->AppendData(kResponseBody);
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_context_.AddFinishedBlob(blob_data.get());
  helper_->RespondWithBlob(blob_handle->uuid(), blob_handle->size());

  // Perform the request.
  TestRequest();
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

// TODO(falken): Add tests for stream response, network fallback, etc. Basically
// everything in ServiceWorkerURLRequestJobTest.

}  // namespace content
