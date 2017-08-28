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

  // Tells this helper to respond to fetch events with the specified stream.
  void RespondWithStream(
      blink::mojom::ServiceWorkerStreamCallbackRequest callback_request,
      mojo::ScopedDataPipeConsumerHandle consumer_handle) {
    response_mode_ = ResponseMode::kStream;
    stream_handle_ = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle_->callback_request = std::move(callback_request);
    stream_handle_->stream = std::move(consumer_handle);
  }

  // Tells this helper to respond to fetch events with network fallback.
  // i.e.,simulate the service worker not calling respondWith().
  void RespondWithFallback() {
    response_mode_ = ResponseMode::kFallbackResponse;
  }

  // Tells this helper to simulate failure to dispatch the fetch event to the
  // service worker.
  void FailToDispatchFetchEvent() {
    response_mode_ = ResponseMode::kFailFetchEventDispatch;
  }

  // Tells this helper to simulate "early response", where the respondWith()
  // promise resolves before the waitUntil() promise. In this mode, the
  // helper sets the response mode to "early response", which simulates the
  // promise passed to respondWith() resolving before the waitUntil() promise
  // resolves. In this mode, the helper will respond to fetch events
  // immediately, but will not finish the fetch event until FinishWaitUntil() is
  // called.
  void RespondEarly() { response_mode_ = ResponseMode::kEarlyResponse; }
  void FinishWaitUntil() {
    std::move(finish_callback_).Run(SERVICE_WORKER_OK, base::Time::Now());
    base::RunLoop().RunUntilIdle();
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
      case ResponseMode::kStream:
        response_callback->OnResponseStream(
            ServiceWorkerResponse(
                base::MakeUnique<std::vector<GURL>>(), 200, "OK",
                network::mojom::FetchResponseType::kDefault,
                base::MakeUnique<ServiceWorkerHeaderMap>(), "" /* blob_uuid */,
                0 /* blob_size */, nullptr /* blob */,
                blink::kWebServiceWorkerResponseErrorUnknown, base::Time(),
                false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                base::MakeUnique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            std::move(stream_handle_), base::Time::Now());
        std::move(finish_callback).Run(SERVICE_WORKER_OK, base::Time::Now());
        return;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(base::Time::Now());
        std::move(finish_callback).Run(SERVICE_WORKER_OK, base::Time::Now());
        return;
      case ResponseMode::kFailFetchEventDispatch:
        // Simulate failure by stopping the worker before the event finishes.
        // This causes ServiceWorkerVersion::StartRequest() to call its error
        // callback, which triggers ServiceWorkerURLLoaderJob's dispatch failed
        // behavior.
        SimulateWorkerStopped(embedded_worker_id);
        // Finish the event by calling |finish_callback|.
        // This is the Mojo callback for
        // mojom::ServiceWorkerEventDispatcher::DispatchFetchEvent().
        // If this is not called, Mojo will complain. In production code,
        // ServiceWorkerContextClient would call this when it aborts all
        // callbacks after an unexpected stop.
        std::move(finish_callback)
            .Run(SERVICE_WORKER_ERROR_ABORT, base::Time::Now());
        return;
      case ResponseMode::kEarlyResponse:
        finish_callback_ = std::move(finish_callback);
        response_callback->OnResponse(
            ServiceWorkerResponse(
                base::MakeUnique<std::vector<GURL>>(), 200, "OK",
                network::mojom::FetchResponseType::kDefault,
                base::MakeUnique<ServiceWorkerHeaderMap>(), "" /* blob_uuid */,
                0 /* blob_size */, nullptr /* blob */,
                blink::kWebServiceWorkerResponseErrorUnknown, base::Time(),
                false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                base::MakeUnique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            base::Time::Now());
        // Now the caller must call FinishWaitUntil() to finish the event.
        return;
    }
    NOTREACHED();
  }

 private:
  enum class ResponseMode {
    kDefault,
    kBlob,
    kStream,
    kFallbackResponse,
    kFailFetchEventDispatch,
    kEarlyResponse
  };

  ResponseMode response_mode_ = ResponseMode::kDefault;

  // For ResponseMode::kBlob.
  std::string blob_uuid_;
  uint64_t blob_size_ = 0;

  // For ResponseMode::kStream.
  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

  // For ResponseMode::kEarlyResponse.
  FetchCallback finish_callback_;

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

  // Indicates whether ServiceWorkerURLLoaderJob decided to handle a request,
  // i.e., it returned a non-null StartLoaderCallback for the request.
  enum class JobResult {
    kHandledRequest,
    kDidNotHandleRequest,
  };

  // Performs a request. When this returns, |client_| will have information
  // about the response.
  JobResult TestRequest() {
    ResourceRequest request;
    request.url = GURL("https://www.example.com/");
    request.method = "GET";

    // Start a ServiceWorkerURLLoaderJob. It should return a
    // StartLoaderCallback.
    StartLoaderCallback callback;
    job_ = base::MakeUnique<ServiceWorkerURLLoaderJob>(
        base::BindOnce(&ReceiveStartLoaderCallback, &callback), this, request,
        GetBlobStorageContext());
    job_->ForwardToServiceWorker();
    base::RunLoop().RunUntilIdle();
    if (!callback)
      return JobResult::kDidNotHandleRequest;

    // Start the loader. It will load |request.url|.
    mojom::URLLoaderPtr loader;
    std::move(callback).Run(mojo::MakeRequest(&loader),
                            client_.CreateInterfacePtr());
    client_.RunUntilComplete();

    return JobResult::kHandledRequest;
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

  void MainResourceLoadFailed() override {
    was_main_resource_load_failed_called_ = true;
  }
  // --------------------------------------------------------------------------

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<Helper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  storage::BlobStorageContext blob_context_;
  TestURLLoaderClient client_;
  bool was_main_resource_load_failed_called_ = false;
  std::unique_ptr<ServiceWorkerURLLoaderJob> job_;
};

TEST_F(ServiceWorkerURLLoaderJobTest, Basic) {
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
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
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
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

// Tell the helper to respond with a non-existent Blob.
TEST_F(ServiceWorkerURLLoaderJobTest, NonExistentBlobUUIDResponse) {
  helper_->RespondWithBlob("blob-id:nothing-is-here", 0);

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
  const ResourceResponseHead& info = client_.response_head();
  // TODO(falken): Currently our code returns 404 not found (with net::OK), but
  // the spec seems to say this should act as if a network error has occurred.
  // See https://crbug.com/732750
  EXPECT_EQ(404, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);
}

TEST_F(ServiceWorkerURLLoaderJobTest, StreamResponse) {
  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);

  // TODO(falken): This should be true since the worker is still streaming the
  // response body. See https://crbug.com/758455
  EXPECT_FALSE(version_->HasWork());

  // Write the body stream.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

// Test when a stream response body is aborted.
TEST_F(ServiceWorkerURLLoaderJobTest, StreamResponse_Abort) {
  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);

  // Start writing the body stream, then abort before finishing.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnAborted();
  data_pipe.producer_handle.reset();
  // TODO(falken): This should be an error, see https://crbug.com/758455
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

// Test when the job is cancelled while a stream response is being written.
TEST_F(ServiceWorkerURLLoaderJobTest, StreamResponseAndCancel) {
  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);

  // Start writing the body stream, then cancel the job before finishing.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  EXPECT_TRUE(data_pipe.producer_handle.is_valid());
  EXPECT_FALSE(job_->WasCanceled());
  // TODO(falken): This should be true since the worker is still streaming the
  // response body. See https://crbug.com/758455
  EXPECT_FALSE(version_->HasWork());
  job_->Cancel();
  EXPECT_TRUE(job_->WasCanceled());
  EXPECT_FALSE(version_->HasWork());

  // Although ServiceworkerURLLoaderJob resets its URLLoaderClient pointer in
  // Cancel(), the URLLoaderClient still exists. In this test, it is |client_|
  // which owns the data pipe, so it's still valid to write data to it.
  mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  // TODO(falken): This should probably be an error.
  EXPECT_EQ(MOJO_RESULT_OK, mojo_result);

  stream_callback->OnAborted();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_pipe.consumer_handle.is_valid());
  // TODO(falken): This should be an error, see https://crbug.com/758455
  EXPECT_EQ(net::OK, client_.completion_status().error_code);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerURLLoaderJobTest, FallbackResponse) {
  helper_->RespondWithFallback();

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kDidNotHandleRequest, result);

  // The request should not be handled by the job, but it shouldn't be a
  // failure.
  EXPECT_FALSE(was_main_resource_load_failed_called_);
}

// Test when dispatching the fetch event to the service worker failed.
TEST_F(ServiceWorkerURLLoaderJobTest, FailFetchDispatch) {
  helper_->FailToDispatchFetchEvent();

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kDidNotHandleRequest, result);
  EXPECT_TRUE(was_main_resource_load_failed_called_);
}

// Test when the respondWith() promise resolves before the waitUntil() promise
// resolves. The response should be received before the event finishes.
TEST_F(ServiceWorkerURLLoaderJobTest, EarlyResponse) {
  helper_->RespondEarly();

  // Perform the request.
  JobResult result = TestRequest();
  EXPECT_EQ(JobResult::kHandledRequest, result);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectFetchedViaServiceWorker(info);

  // Although the response was already received, the event remains outstanding
  // until waitUntil() resolves.
  EXPECT_TRUE(version_->HasWork());
  helper_->FinishWaitUntil();
  EXPECT_FALSE(version_->HasWork());
}

// Test asking the job to fallback to network. In production code, this happens
// when there is no active service worker for the URL, or it must be skipped,
// etc.
TEST_F(ServiceWorkerURLLoaderJobTest, FallbackToNetwork) {
  ResourceRequest request;
  request.url = GURL("https://www.example.com/");
  request.method = "GET";

  StartLoaderCallback callback;
  auto job = base::MakeUnique<ServiceWorkerURLLoaderJob>(
      base::BindOnce(&ReceiveStartLoaderCallback, &callback), this, request,
      GetBlobStorageContext());
  // Ask the job to fallback to network. In production code,
  // ServiceWorkerControlleeRequestHandler calls FallbackToNetwork() to do this.
  job->FallbackToNetwork();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback);
}

}  // namespace content
