// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_loader_job.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/public/interfaces/blobs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

void ReceiveStartLoaderCallback(StartLoaderCallback* out_callback,
                                StartLoaderCallback callback) {
  *out_callback = std::move(callback);
}

// NavigationPreloadLoaderClient mocks the renderer-side URLLoaderClient for the
// navigation preload network request performed by the browser. In production
// code, this is ServiceWorkerContextClient::NavigationPreloadRequest,
// which it forwards the response to FetchEvent#preloadResponse. Here, it
// simulates passing the response to FetchEvent#respondWith.
//
// The navigation preload test is quite involved. The flow of data is:
// 1. ServiceWorkerURLLoaderJob asks ServiceWorkerFetchDispatcher to start
//    navigation preload.
// 2. ServiceWorkerFetchDispatcher starts the network request which is mocked
//    by MockNetworkURLLoaderFactory. The response is sent to
//    ServiceWorkerFetchDispatcher::DelegatingURLLoaderClient.
// 3. DelegatingURLLoaderClient sends the response to the |preload_handle|
//    that was passed to Helper::OnFetchEvent().
// 4. Helper::OnFetchEvent() creates NavigationPreloadLoaderClient, which
//    receives the response.
// 5. NavigationPreloadLoaderClient calls OnFetchEvent()'s callbacks
//    with the response.
// 6. Like all FetchEvent responses, the response is sent to
//    ServiceWorkerURLLoaderJob::DidDispatchFetchEvent, and the
//    StartLoaderCallback is returned.
class NavigationPreloadLoaderClient final : public mojom::URLLoaderClient {
 public:
  NavigationPreloadLoaderClient(
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorkerEventDispatcher::DispatchFetchEventCallback
          finish_callback)
      : url_loader_(std::move(preload_handle->url_loader)),
        binding_(this, std::move(preload_handle->url_loader_client_request)),
        response_callback_(std::move(response_callback)),
        finish_callback_(std::move(finish_callback)) {
    binding_.set_connection_error_handler(
        base::BindOnce(&NavigationPreloadLoaderClient::OnConnectionError,
                       base::Unretained(this)));
  }
  ~NavigationPreloadLoaderClient() override = default;

  // mojom::URLLoaderClient implementation
  void OnReceiveResponse(
      const ResourceResponseHead& response_head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    response_head_ = response_head;
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    body_ = std::move(body);
    // We could call OnResponseStream() here, but for simplicity, don't do
    // anything until OnComplete().
  }
  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
    auto stream_handle = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle->callback_request = mojo::MakeRequest(&stream_callback);
    stream_handle->stream = std::move(body_);

    // Simulate passing the navigation preload response to
    // FetchEvent#respondWith.
    response_callback_->OnResponseStream(
        ServiceWorkerResponse(
            base::MakeUnique<std::vector<GURL>>(
                response_head_.url_list_via_service_worker),
            response_head_.headers->response_code(),
            response_head_.headers->GetStatusText(),
            response_head_.response_type_via_service_worker,
            base::MakeUnique<ServiceWorkerHeaderMap>(), "" /* blob_uuid */,
            0 /* blob_size */, nullptr /* blob */,
            blink::kWebServiceWorkerResponseErrorUnknown, base::Time(),
            false /* response_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            base::MakeUnique<
                ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
        std::move(stream_handle), base::Time::Now());
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::Time::Now());
    stream_callback->OnCompleted();
    delete this;
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {}
  void OnDataDownloaded(int64_t data_length,
                        int64_t encoded_data_length) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}

  void OnConnectionError() { delete this; }

 private:
  mojom::URLLoaderPtr url_loader_;
  mojo::Binding<mojom::URLLoaderClient> binding_;

  ResourceResponseHead response_head_;
  mojo::ScopedDataPipeConsumerHandle body_;

  // Callbacks that complete Helper::OnFetchEvent().
  mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_;
  mojom::ServiceWorkerEventDispatcher::DispatchFetchEventCallback
      finish_callback_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPreloadLoaderClient);
};

// A URLLoaderFactory that returns 200 OK with a simple body to any request.
//
// ServiceWorkerURLLoaderJobTest sets the network factory for
// ServiceWorkerContextCore to MockNetworkURLLoaderFactory. So far, it's only
// used for navigation preload in these tests.
class MockNetworkURLLoaderFactory final : public mojom::URLLoaderFactory {
 public:
  MockNetworkURLLoaderFactory() = default;

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    std::string headers = "HTTP/1.1 200 OK\n\n";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
    ResourceResponseHead response;
    response.headers = info.headers;
    response.headers->GetMimeType(&response.mime_type);
    client->OnReceiveResponse(response, base::nullopt, nullptr);

    std::string body = "this body came from the network";
    uint32_t bytes_written = body.size();
    mojo::DataPipe data_pipe;
    data_pipe.producer_handle->WriteData(body.data(), &bytes_written,
                                         MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    ResourceRequestCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
  }

  void Clone(mojom::URLLoaderFactoryRequest factory) override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkURLLoaderFactory);
};

// Helper simulates a service worker handling fetch events. The response can be
// customized via RespondWith* functions.
class Helper : public EmbeddedWorkerTestHelper {
 public:
  Helper()
      : EmbeddedWorkerTestHelper(
            base::FilePath(),
            base::MakeRefCounted<URLLoaderFactoryGetter>()) {
    mojom::URLLoaderFactoryPtr mock_loader_factory;
    mojo::MakeStrongBinding(base::MakeUnique<MockNetworkURLLoaderFactory>(),
                            MakeRequest(&mock_loader_factory));
    url_loader_factory_getter()->SetNetworkFactoryForTesting(
        std::move(mock_loader_factory));
  }
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
  // i.e., simulate the service worker not calling respondWith().
  void RespondWithFallback() {
    response_mode_ = ResponseMode::kFallbackResponse;
  }

  // Tells this helper to respond to fetch events with
  // FetchEvent#preloadResponse. See NavigationPreloadLoaderClient's
  // documentation for details.
  void RespondWithNavigationPreloadResponse() {
    response_mode_ = ResponseMode::kNavigationPreloadResponse;
  }

  // Tells this helper to respond to fetch events with the redirect response.
  void RespondWithRedirectResponse(const GURL& new_url) {
    response_mode_ = ResponseMode::kRedirect;
    redirected_url_ = new_url;
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
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::Time::Now());
    base::RunLoop().RunUntilIdle();
  }

  void ReadRequestBody(std::string* out_string) {
    ASSERT_TRUE(request_body_blob_);

    mojo::DataPipe pipe;
    (*request_body_blob_)->ReadAll(std::move(pipe.producer_handle), nullptr);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(mojo::common::BlockingCopyToString(
        std::move(pipe.consumer_handle), out_string));
  }

 protected:
  void OnFetchEvent(
      int embedded_worker_id,
      int fetch_event_id,
      const ServiceWorkerFetchRequest& request,
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorkerEventDispatcher::DispatchFetchEventCallback
          finish_callback) override {
    request_body_blob_ = request.blob;
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
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::Time::Now());
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
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::Time::Now());
        return;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(base::Time::Now());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::Time::Now());
        return;
      case ResponseMode::kNavigationPreloadResponse:
        // Deletes itself when done.
        new NavigationPreloadLoaderClient(std::move(preload_handle),
                                          std::move(response_callback),
                                          std::move(finish_callback));
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
            .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED,
                 base::Time::Now());
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
      case ResponseMode::kRedirect:
        auto headers = base::MakeUnique<ServiceWorkerHeaderMap>();
        (*headers)["location"] = redirected_url_.spec();
        response_callback->OnResponse(
            ServiceWorkerResponse(
                base::MakeUnique<std::vector<GURL>>(), 301, "Moved Permanently",
                network::mojom::FetchResponseType::kDefault, std::move(headers),
                "" /* blob_uuid */, 0 /* blob_size */, nullptr /* blob */,
                blink::kWebServiceWorkerResponseErrorUnknown, base::Time(),
                false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                base::MakeUnique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            base::Time::Now());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::Time::Now());
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
    kNavigationPreloadResponse,
    kFailFetchEventDispatch,
    kEarlyResponse,
    kRedirect
  };

  ResponseMode response_mode_ = ResponseMode::kDefault;
  scoped_refptr<storage::BlobHandle> request_body_blob_;

  // For ResponseMode::kBlob.
  std::string blob_uuid_;
  uint64_t blob_size_ = 0;

  // For ResponseMode::kStream.
  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

  // For ResponseMode::kEarlyResponse.
  mojom::ServiceWorkerEventDispatcher::DispatchFetchEventCallback
      finish_callback_;

  // For ResponseMode::kRedirect.
  GURL redirected_url_;

  DISALLOW_COPY_AND_ASSIGN(Helper);
};

// Returns typical response info for a resource load that went through a service
// worker.
std::unique_ptr<ResourceResponseHead> CreateResponseInfoFromServiceWorker() {
  auto head = std::make_unique<ResourceResponseHead>();
  head->was_fetched_via_service_worker = true;
  head->was_fallback_required_by_service_worker = false;
  head->url_list_via_service_worker = std::vector<GURL>();
  head->response_type_via_service_worker =
      network::mojom::FetchResponseType::kDefault;
  head->is_in_cache_storage = false;
  head->cache_storage_cache_name = std::string();
  head->did_service_worker_navigation_preload = false;
  return head;
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
        helper_(base::MakeUnique<Helper>()) {}
  ~ServiceWorkerURLLoaderJobTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kNetworkService);

    // Create an active service worker.
    storage()->LazyInitializeForTest(base::BindOnce(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    registration_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(
            GURL("https://example.com/")),
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

  // Returns whether ServiceWorkerURLLoaderJob handled the request. If
  // kHandledRequest was returned, the request is ongoing and the caller can use
  // functions like client_.RunUntilComplete() to wait for completion.
  JobResult StartRequest(std::unique_ptr<ResourceRequest> request) {
    // Start a ServiceWorkerURLLoaderJob. It should return a
    // StartLoaderCallback.
    StartLoaderCallback callback;
    job_ = base::MakeUnique<ServiceWorkerURLLoaderJob>(
        base::BindOnce(&ReceiveStartLoaderCallback, &callback), this, *request,
        base::WrapRefCounted<URLLoaderFactoryGetter>(
            helper_->context()->loader_factory_getter()),
        GetBlobStorageContext());
    job_->ForwardToServiceWorker();
    base::RunLoop().RunUntilIdle();
    if (!callback)
      return JobResult::kDidNotHandleRequest;

    // Start the loader. It will load |request.url|.
    std::move(callback).Run(mojo::MakeRequest(&loader_),
                            client_.CreateInterfacePtr());

    return JobResult::kHandledRequest;
  }

  void ExpectResponseInfo(const ResourceResponseHead& info,
                          const ResourceResponseHead& expected_info) {
    EXPECT_EQ(expected_info.was_fetched_via_service_worker,
              info.was_fetched_via_service_worker);
    EXPECT_EQ(expected_info.was_fallback_required_by_service_worker,
              info.was_fallback_required_by_service_worker);
    EXPECT_EQ(expected_info.url_list_via_service_worker,
              info.url_list_via_service_worker);
    EXPECT_EQ(expected_info.response_type_via_service_worker,
              info.response_type_via_service_worker);
    EXPECT_FALSE(info.service_worker_start_time.is_null());
    EXPECT_FALSE(info.service_worker_ready_time.is_null());
    EXPECT_LT(info.service_worker_start_time, info.service_worker_ready_time);
    EXPECT_EQ(expected_info.is_in_cache_storage, info.is_in_cache_storage);
    EXPECT_EQ(expected_info.cache_storage_cache_name,
              info.cache_storage_cache_name);
    EXPECT_EQ(expected_info.did_service_worker_navigation_preload,
              info.did_service_worker_navigation_preload);
  }

  std::unique_ptr<ResourceRequest> CreateRequest() {
    std::unique_ptr<ResourceRequest> request =
        base::MakeUnique<ResourceRequest>();
    request->url = GURL("https://www.example.com/");
    request->method = "GET";
    request->fetch_request_mode = FETCH_REQUEST_MODE_NAVIGATE;
    request->fetch_credentials_mode = FETCH_CREDENTIALS_MODE_INCLUDE;
    request->fetch_redirect_mode = FetchRedirectMode::MANUAL_MODE;
    return request;
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
  mojom::URLLoaderPtr loader_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServiceWorkerURLLoaderJobTest, Basic) {
  // Perform the request
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());
}

// Test that the request body is passed to the fetch event.
TEST_F(ServiceWorkerURLLoaderJobTest, RequestBody) {
  const std::string kData = "BlobRequest";

  // Create a request with a body.
  auto request_body = base::MakeRefCounted<ResourceRequestBody>();
  request_body->AppendBytes(kData.c_str(), kData.length());
  std::unique_ptr<ResourceRequest> request = CreateRequest();
  request->request_body = request_body;

  // This test doesn't use the response to the fetch event, so just have the
  // service worker do simple network fallback.
  helper_->RespondWithFallback();
  JobResult result = StartRequest(std::move(request));
  EXPECT_EQ(JobResult::kDidNotHandleRequest, result);

  // Verify that the request body was passed to the fetch event.
  std::string body;
  helper_->ReadRequestBody(&body);
  EXPECT_EQ(kData, body);
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
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilComplete();

  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Test the body.
  std::string body;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &body));
  EXPECT_EQ(kResponseBody, body);
}

// Tell the helper to respond with a non-existent Blob.
TEST_F(ServiceWorkerURLLoaderJobTest, NonExistentBlobUUIDResponse) {
  helper_->RespondWithBlob("blob-id:nothing-is-here", 0);

  // Perform the request.
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilComplete();

  const ResourceResponseHead& info = client_.response_head();
  // TODO(falken): Currently our code returns 404 not found (with net::OK), but
  // the spec seems to say this should act as if a network error has occurred.
  // See https://crbug.com/732750
  EXPECT_EQ(404, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());
}

TEST_F(ServiceWorkerURLLoaderJobTest, StreamResponse) {
  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilResponseReceived();

  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  EXPECT_TRUE(version_->HasWork());

  // Write the body stream.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();

  client_.RunUntilComplete();
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
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilResponseReceived();

  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then abort before finishing.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnAborted();
  data_pipe.producer_handle.reset();

  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

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
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilResponseReceived();

  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then cancel the job before finishing.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  EXPECT_TRUE(data_pipe.producer_handle.is_valid());
  EXPECT_FALSE(job_->WasCanceled());
  EXPECT_TRUE(version_->HasWork());
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

  client_.RunUntilComplete();
  EXPECT_FALSE(data_pipe.consumer_handle.is_valid());
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerURLLoaderJobTest, FallbackResponse) {
  helper_->RespondWithFallback();

  // Perform the request.
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kDidNotHandleRequest, result);

  // The request should not be handled by the job, but it shouldn't be a
  // failure.
  EXPECT_FALSE(was_main_resource_load_failed_called_);
}

// Test when dispatching the fetch event to the service worker failed.
TEST_F(ServiceWorkerURLLoaderJobTest, FailFetchDispatch) {
  helper_->FailToDispatchFetchEvent();

  // Perform the request.
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kDidNotHandleRequest, result);
  EXPECT_TRUE(was_main_resource_load_failed_called_);
}

// Test when the respondWith() promise resolves before the waitUntil() promise
// resolves. The response should be received before the event finishes.
TEST_F(ServiceWorkerURLLoaderJobTest, EarlyResponse) {
  helper_->RespondEarly();

  // Perform the request.
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilComplete();

  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

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
  request.fetch_request_mode = FETCH_REQUEST_MODE_NAVIGATE;
  request.fetch_credentials_mode = FETCH_CREDENTIALS_MODE_INCLUDE;
  request.fetch_redirect_mode = FetchRedirectMode::MANUAL_MODE;

  StartLoaderCallback callback;
  auto job = base::MakeUnique<ServiceWorkerURLLoaderJob>(
      base::BindOnce(&ReceiveStartLoaderCallback, &callback), this, request,
      base::WrapRefCounted<URLLoaderFactoryGetter>(
          helper_->context()->loader_factory_getter()),
      GetBlobStorageContext());
  // Ask the job to fallback to network. In production code,
  // ServiceWorkerControlleeRequestHandler calls FallbackToNetwork() to do this.
  job->FallbackToNetwork();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback);
}

// Test responding to the fetch event with the navigation preload response.
TEST_F(ServiceWorkerURLLoaderJobTest, NavigationPreload) {
  registration_->EnableNavigationPreload(true);
  helper_->RespondWithNavigationPreloadResponse();

  // Perform the request
  JobResult result = StartRequest(CreateRequest());
  ASSERT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());

  std::unique_ptr<ResourceResponseHead> expected_info =
      CreateResponseInfoFromServiceWorker();
  expected_info->did_service_worker_navigation_preload = true;
  ExpectResponseInfo(info, *expected_info);

  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &response));
  EXPECT_EQ("this body came from the network", response);
}

// Test responding to the fetch event with a redirect response.
TEST_F(ServiceWorkerURLLoaderJobTest, Redirect) {
  GURL new_url("https://example.com/redirected");
  helper_->RespondWithRedirectResponse(new_url);

  // Perform the request.
  JobResult result = StartRequest(CreateRequest());
  EXPECT_EQ(JobResult::kHandledRequest, result);
  client_.RunUntilRedirectReceived();

  const ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(301, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  const net::RedirectInfo& redirect_info = client_.redirect_info();
  EXPECT_EQ(301, redirect_info.status_code);
  EXPECT_EQ("GET", redirect_info.new_method);
  EXPECT_EQ(new_url, redirect_info.new_url);
}

}  // namespace content
