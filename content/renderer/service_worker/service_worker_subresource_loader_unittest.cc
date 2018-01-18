// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_subresource_loader.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "content/renderer/loader/child_url_loader_factory_getter_impl.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

namespace {

// This class need to set ChildURLLoaderFactoryGetter. CreateLoaderAndStart()
// need to implement. todo(emim): Merge this and the one in
// service_worker_url_loader_job_unittest.cc.
class FakeNetworkURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  FakeNetworkURLLoaderFactory() = default;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    std::string headers = "HTTP/1.1 200 OK\n\n";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
    network::ResourceResponseHead response;
    response.headers = info.headers;
    response.headers->GetMimeType(&response.mime_type);
    client->OnReceiveResponse(response, base::nullopt, nullptr);

    std::string body = "this body came from the network";
    uint32_t bytes_written = body.size();
    mojo::DataPipe data_pipe;
    data_pipe.producer_handle->WriteData(body.data(), &bytes_written,
                                         MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest factory) override {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkURLLoaderFactory);
};

class FakeControllerServiceWorker : public mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;
  ~FakeControllerServiceWorker() override = default;

  void CloseAllBindings() { bindings_.CloseAllBindings(); }

  // Tells this controller to abort the fetch event without a response.
  // i.e., simulate the service worker failing to handle the fetch event.
  void AbortEventWithNoResponse() { response_mode_ = ResponseMode::kAbort; }

  // Tells this controller to respond to fetch events with network fallback.
  // i.e., simulate the service worker not calling respondWith().
  void RespondWithFallback() {
    response_mode_ = ResponseMode::kFallbackResponse;
  }

  // Tells this controller to respond to fetch events with the specified stream.
  void RespondWithStream(
      blink::mojom::ServiceWorkerStreamCallbackRequest callback_request,
      mojo::ScopedDataPipeConsumerHandle consumer_handle) {
    response_mode_ = ResponseMode::kStream;
    stream_handle_ = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle_->callback_request = std::move(callback_request);
    stream_handle_->stream = std::move(consumer_handle);
  }

  // Tells this controller to respond to fetch events with a error response.
  void RespondWithError() { response_mode_ = ResponseMode::kErrorResponse; }

  // Tells this controller to respond to fetch events with a redirect response.
  void RespondWithRedirect(const std::string& redirect_location_header) {
    response_mode_ = ResponseMode::kRedirectResponse;
    redirect_location_header_ = redirect_location_header;
  }

  void ReadRequestBody(std::string* out_string) {
    ASSERT_TRUE(request_body_);
    const std::vector<network::DataElement>* elements =
        request_body_->elements();
    // So far this test expects a single bytes element.
    ASSERT_EQ(1u, elements->size());
    const network::DataElement& element = elements->front();
    ASSERT_EQ(network::DataElement::TYPE_BYTES, element.type());
    *out_string = std::string(element.bytes(), element.length());
  }

  // mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      const network::ResourceRequest& request,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    EXPECT_FALSE(ServiceWorkerUtils::IsMainResourceType(
        static_cast<ResourceType>(request.resource_type)));
    request_body_ = request.request_body;

    fetch_event_count_++;
    fetch_event_request_ = request;
    switch (response_mode_) {
      case ResponseMode::kDefault:
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED, base::Time());
        break;
      case ResponseMode::kAbort:
        std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::ABORTED,
                                base::Time());
        break;
      case ResponseMode::kStream:
        response_callback->OnResponseStream(
            ServiceWorkerResponse(
                std::make_unique<std::vector<GURL>>(), 200, "OK",
                network::mojom::FetchResponseType::kDefault,
                std::make_unique<ServiceWorkerHeaderMap>(), "" /* blob_uuid */,
                0 /* blob_size */, nullptr /* blob */,
                blink::mojom::ServiceWorkerResponseError::kUnknown,
                base::Time(), false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                std::make_unique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            std::move(stream_handle_), base::Time::Now());
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED, base::Time());
        break;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(base::Time::Now());
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED,
            base::Time::Now());
        break;
      case ResponseMode::kErrorResponse:
        response_callback->OnResponse(
            ServiceWorkerResponse(
                std::make_unique<std::vector<GURL>>(), 0 /* status_code */,
                "" /* status_text */,
                network::mojom::FetchResponseType::kDefault,
                std::make_unique<ServiceWorkerHeaderMap>(), "" /* blob_uuid */,
                0 /* blob_size */, nullptr /* blob */,
                blink::mojom::ServiceWorkerResponseError::kPromiseRejected,
                base::Time(), false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                std::make_unique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            base::Time::Now());
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::REJECTED,
            base::Time::Now());
        break;
      case ResponseMode::kRedirectResponse: {
        auto headers = std::make_unique<ServiceWorkerHeaderMap>();
        (*headers)["Location"] = redirect_location_header_;
        response_callback->OnResponse(
            ServiceWorkerResponse(
                std::make_unique<std::vector<GURL>>(), 302, "Found",
                network::mojom::FetchResponseType::kDefault, std::move(headers),
                "" /* blob_uuid */, 0 /* blob_size */, nullptr /* blob */,
                blink::mojom::ServiceWorkerResponseError::kUnknown,
                base::Time(), false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                std::make_unique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            base::Time::Now());
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED, base::Time());
      } break;
    }
    if (fetch_event_callback_)
      std::move(fetch_event_callback_).Run();
  }

  void Clone(mojom::ControllerServiceWorkerRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void RunUntilFetchEvent() {
    base::RunLoop loop;
    fetch_event_callback_ = loop.QuitClosure();
    loop.Run();
  }

  int fetch_event_count() const { return fetch_event_count_; }
  const network::ResourceRequest& fetch_event_request() const {
    return fetch_event_request_;
  }

 private:
  enum class ResponseMode {
    kDefault,
    kAbort,
    kStream,
    kFallbackResponse,
    kErrorResponse,
    kRedirectResponse
  };

  ResponseMode response_mode_ = ResponseMode::kDefault;
  scoped_refptr<network::ResourceRequestBody> request_body_;

  int fetch_event_count_ = 0;
  network::ResourceRequest fetch_event_request_;
  base::OnceClosure fetch_event_callback_;
  mojo::BindingSet<mojom::ControllerServiceWorker> bindings_;

  // For ResponseMode::kStream.
  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

  // For ResponseMode::kRedirectResponse
  std::string redirect_location_header_;

  DISALLOW_COPY_AND_ASSIGN(FakeControllerServiceWorker);
};

class FakeServiceWorkerContainerHost
    : public mojom::ServiceWorkerContainerHost {
 public:
  explicit FakeServiceWorkerContainerHost(
      FakeControllerServiceWorker* fake_controller)
      : fake_controller_(fake_controller) {}

  ~FakeServiceWorkerContainerHost() override = default;

  void set_fake_controller(FakeControllerServiceWorker* new_fake_controller) {
    fake_controller_ = new_fake_controller;
  }

  int get_controller_service_worker_count() const {
    return get_controller_service_worker_count_;
  }

 private:
  // Implements mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                RegisterCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrations(GetRegistrationsCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetControllerServiceWorker(
      mojom::ControllerServiceWorkerRequest request) override {
    get_controller_service_worker_count_++;
    if (!fake_controller_)
      return;
    fake_controller_->Clone(std::move(request));
  }
  void CloneForWorker(
      mojom::ServiceWorkerContainerHostRequest request) override {
    NOTIMPLEMENTED();
  }

  int get_controller_service_worker_count_ = 0;
  FakeControllerServiceWorker* fake_controller_;
  DISALLOW_COPY_AND_ASSIGN(FakeServiceWorkerContainerHost);
};

}  // namespace


class ServiceWorkerSubresourceLoaderTest : public ::testing::Test {
 protected:
  ServiceWorkerSubresourceLoaderTest()
      : fake_container_host_(&fake_controller_) {}
  ~ServiceWorkerSubresourceLoaderTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kNetworkService);

    network::mojom::URLLoaderFactoryPtr fake_loader_factory;
    mojo::MakeStrongBinding(std::make_unique<FakeNetworkURLLoaderFactory>(),
                            MakeRequest(&fake_loader_factory));
    loader_factory_getter_ =
        base::MakeRefCounted<ChildURLLoaderFactoryGetterImpl>(
            std::move(fake_loader_factory), nullptr);
  }

  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory>
  CreateSubresourceLoaderFactory() {
    if (!connector_) {
      connector_ = base::MakeRefCounted<ControllerServiceWorkerConnector>(
          &fake_container_host_);
    }
    return std::make_unique<ServiceWorkerSubresourceLoaderFactory>(
        connector_, loader_factory_getter_);
  }

  // Starts |request| using |loader_factory| and sets |out_loader| and
  // |out_loader_client| to the resulting URLLoader and its URLLoaderClient. The
  // caller can then use functions like client.RunUntilComplete() to wait for
  // completion. Calling fake_controller_->RunUntilFetchEvent() also advances
  // the load to until |fake_controller_| receives the fetch event.
  void StartRequest(ServiceWorkerSubresourceLoaderFactory* loader_factory,
                    const network::ResourceRequest& request,
                    network::mojom::URLLoaderPtr* out_loader,
                    std::unique_ptr<TestURLLoaderClient>* out_loader_client) {
    *out_loader_client = std::make_unique<TestURLLoaderClient>();
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(out_loader), 0, 0, network::mojom::kURLLoadOptionNone,
        request, (*out_loader_client)->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  network::ResourceRequest CreateRequest(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
    return request;
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<ChildURLLoaderFactoryGetter> loader_factory_getter_;
  scoped_refptr<ControllerServiceWorkerConnector> connector_;

  FakeServiceWorkerContainerHost fake_container_host_;
  FakeControllerServiceWorker fake_controller_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderTest);
};

TEST_F(ServiceWorkerSubresourceLoaderTest, Basic) {
  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  fake_controller_.RunUntilFetchEvent();

  EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
  EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
  EXPECT_EQ(1, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
}

TEST_F(ServiceWorkerSubresourceLoaderTest, Abort) {
  fake_controller_.AbortEventWithNoResponse();

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  client->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController) {
  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Loading another resource reuses the existing connection to the
  // ControllerServiceWorker (i.e. it doesn't increase the get controller
  // service worker count).
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(2, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.CloseAllBindings();
  base::RunLoop().RunUntilIdle();

  {
    // This should re-obtain the ControllerServiceWorker.
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo3.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(3, fake_controller_.fetch_event_count());
    EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, NoController) {
  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Make the connector have no controller.
  connector_->ResetControllerConnection(nullptr);
  base::RunLoop().RunUntilIdle();

  {
    // This should fallback to the network.
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    client->RunUntilComplete();

    EXPECT_TRUE(client->has_received_completion());
    EXPECT_FALSE(client->response_head().was_fetched_via_service_worker);

    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController_RestartFetchEvent) {
  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Loading another resource reuses the existing connection to the
  // ControllerServiceWorker (i.e. it doesn't increase the get controller
  // service worker count).
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(2, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo3.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.CloseAllBindings();
  base::RunLoop().RunUntilIdle();

  // If connection is closed during fetch event, it's restarted and successfully
  // finishes.
  EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
  EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
  EXPECT_EQ(3, fake_controller_.fetch_event_count());
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController_TooManyRestart) {
  // Simulate the container host fails to start a service worker.
  fake_container_host_.set_fake_controller(nullptr);

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);

  // Try to dispatch fetch event to the bad worker.
  base::RunLoop().RunUntilIdle();

  // The request should be failed instead of infinite loop to restart the
  // inflight fetch event.
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, StreamResponse) {
  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  fake_controller_.RespondWithStream(mojo::MakeRequest(&stream_callback),
                                     std::move(data_pipe.consumer_handle));

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  client->RunUntilResponseReceived();

  const network::ResourceResponseHead& info = client->response_head();
  EXPECT_EQ(200, info.headers->response_code());
  EXPECT_EQ(true, info.was_fetched_via_service_worker);
  EXPECT_EQ(false, info.was_fallback_required_by_service_worker);
  EXPECT_EQ(std::vector<GURL>(), info.url_list_via_service_worker);
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info.response_type_via_service_worker);
  EXPECT_EQ(false, info.is_in_cache_storage);
  EXPECT_EQ(std::string(), info.cache_storage_cache_name);
  EXPECT_EQ(false, info.did_service_worker_navigation_preload);
  EXPECT_NE(base::TimeTicks(), info.service_worker_start_time);
  EXPECT_NE(base::TimeTicks(), info.service_worker_ready_time);

  // Write the body stream.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerSubresourceLoaderTest, FallbackResponse) {
  fake_controller_.RespondWithFallback();

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  client->RunUntilComplete();

  // OnFallback() should complete the network request using network loader.
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_FALSE(client->response_head().was_fetched_via_service_worker);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, ErrorResponse) {
  fake_controller_.RespondWithError();

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  client->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, RedirectResponse) {
  fake_controller_.RespondWithRedirect("https://www.example.com/bar.png");

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  client->RunUntilRedirectReceived();

  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_TRUE(client->has_received_redirect());
  {
    const net::RedirectInfo& redirect_info = client->redirect_info();
    EXPECT_EQ(302, redirect_info.status_code);
    EXPECT_EQ("GET", redirect_info.new_method);
    EXPECT_EQ(GURL("https://www.example.com/bar.png"), redirect_info.new_url);
  }
  client->ClearHasReceivedRedirect();

  // Redirect once more.
  fake_controller_.RespondWithRedirect("https://other.example.com/baz.png");
  loader->FollowRedirect();
  client->RunUntilRedirectReceived();

  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_TRUE(client->has_received_redirect());
  {
    const net::RedirectInfo& redirect_info = client->redirect_info();
    EXPECT_EQ(302, redirect_info.status_code);
    EXPECT_EQ("GET", redirect_info.new_method);
    EXPECT_EQ(GURL("https://other.example.com/baz.png"), redirect_info.new_url);
  }
  client->ClearHasReceivedRedirect();

  // Give the final response.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  fake_controller_.RespondWithStream(mojo::MakeRequest(&stream_callback),
                                     std::move(data_pipe.consumer_handle));
  loader->FollowRedirect();
  client->RunUntilResponseReceived();

  const network::ResourceResponseHead& info = client->response_head();
  EXPECT_EQ(200, info.headers->response_code());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info.response_type_via_service_worker);

  // Write the body stream.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, TooManyRedirects) {
  int count = 1;
  std::string redirect_location =
      std::string("https://www.example.com/redirect_") +
      base::IntToString(count);
  fake_controller_.RespondWithRedirect(redirect_location);
  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);

  // The Fetch spec says: "If request’s redirect count is twenty, return a
  // network error." https://fetch.spec.whatwg.org/#http-redirect-fetch
  // So fetch can follow the redirect response until 20 times.
  static_assert(net::URLRequest::kMaxRedirects == 20,
                "The Fetch spec requires kMaxRedirects to be 20");
  for (; count < net::URLRequest::kMaxRedirects + 1; ++count) {
    client->RunUntilRedirectReceived();

    EXPECT_TRUE(client->has_received_redirect());
    EXPECT_EQ(net::OK, client->completion_status().error_code);
    const net::RedirectInfo& redirect_info = client->redirect_info();
    EXPECT_EQ(302, redirect_info.status_code);
    EXPECT_EQ("GET", redirect_info.new_method);
    EXPECT_EQ(GURL(redirect_location), redirect_info.new_url);

    client->ClearHasReceivedRedirect();

    // Redirect more.
    redirect_location = std::string("https://www.example.com/redirect_") +
                        base::IntToString(count);
    fake_controller_.RespondWithRedirect(redirect_location);
    loader->FollowRedirect();
  }
  client->RunUntilComplete();

  // Fetch can't follow the redirect response 21 times.
  EXPECT_FALSE(client->has_received_redirect());
  EXPECT_EQ(net::ERR_TOO_MANY_REDIRECTS,
            client->completion_status().error_code);
}

// Test when the service worker responds with network fallback to CORS request.
TEST_F(ServiceWorkerSubresourceLoaderTest, CORSFallbackResponse) {
  fake_controller_.RespondWithFallback();

  const GURL kScope("https://www.example.com/");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  struct TestCase {
    network::mojom::FetchRequestMode fetch_request_mode;
    base::Optional<url::Origin> request_initiator;
    bool expected_was_fallback_required_by_service_worker;
  };
  const TestCase kTests[] = {
      {network::mojom::FetchRequestMode::kSameOrigin,
       base::Optional<url::Origin>(), false},
      {network::mojom::FetchRequestMode::kNoCORS, base::Optional<url::Origin>(),
       false},
      {network::mojom::FetchRequestMode::kCORS, base::Optional<url::Origin>(),
       true},
      {network::mojom::FetchRequestMode::kCORSWithForcedPreflight,
       base::Optional<url::Origin>(), true},
      {network::mojom::FetchRequestMode::kNavigate,
       base::Optional<url::Origin>(), false},
      {network::mojom::FetchRequestMode::kSameOrigin,
       url::Origin::Create(GURL("https://www.example.com/")), false},
      {network::mojom::FetchRequestMode::kNoCORS,
       url::Origin::Create(GURL("https://www.example.com/")), false},
      {network::mojom::FetchRequestMode::kCORS,
       url::Origin::Create(GURL("https://www.example.com/")), false},
      {network::mojom::FetchRequestMode::kCORSWithForcedPreflight,
       url::Origin::Create(GURL("https://www.example.com/")), false},
      {network::mojom::FetchRequestMode::kNavigate,
       url::Origin::Create(GURL("https://other.example.com/")), false},
      {network::mojom::FetchRequestMode::kSameOrigin,
       url::Origin::Create(GURL("https://other.example.com/")), false},
      {network::mojom::FetchRequestMode::kNoCORS,
       url::Origin::Create(GURL("https://other.example.com/")), false},
      {network::mojom::FetchRequestMode::kCORS,
       url::Origin::Create(GURL("https://other.example.com/")), true},
      {network::mojom::FetchRequestMode::kCORSWithForcedPreflight,
       url::Origin::Create(GURL("https://other.example.com/")), true},
      {network::mojom::FetchRequestMode::kNavigate,
       url::Origin::Create(GURL("https://other.example.com/")), false}};

  for (const auto& test : kTests) {
    SCOPED_TRACE(
        ::testing::Message()
        << "fetch_request_mode: " << static_cast<int>(test.fetch_request_mode)
        << " request_initiator: "
        << (test.request_initiator ? test.request_initiator->Serialize()
                                   : std::string("null")));
    // Perform the request.
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    request.fetch_request_mode = test.fetch_request_mode;
    request.request_initiator = test.request_initiator;
    network::mojom::URLLoaderPtr loader;
    std::unique_ptr<TestURLLoaderClient> client;
    StartRequest(factory.get(), request, &loader, &client);
    client->RunUntilResponseReceived();

    const network::ResourceResponseHead& info = client->response_head();
    EXPECT_EQ(test.expected_was_fallback_required_by_service_worker,
              info.was_fetched_via_service_worker);
    EXPECT_EQ(test.expected_was_fallback_required_by_service_worker,
              info.was_fallback_required_by_service_worker);
  }
}

// Test that the request body is passed to the fetch event.
TEST_F(ServiceWorkerSubresourceLoaderTest, RequestBody) {
  const GURL kUrl("https://www.example.com");
  std::unique_ptr<ServiceWorkerSubresourceLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Create a request with a body.
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  const std::string kData = "hi this is the request body";
  request_body->AppendBytes(kData.c_str(), kData.length());
  network::ResourceRequest request = CreateRequest(kUrl);
  request.method = "POST";
  request.request_body = request_body;

  // This test doesn't use the response to the fetch event, so just have the
  // service worker do simple network fallback.
  fake_controller_.RespondWithFallback();

  // Perform the request.
  network::mojom::URLLoaderPtr loader;
  std::unique_ptr<TestURLLoaderClient> client;
  StartRequest(factory.get(), request, &loader, &client);
  fake_controller_.RunUntilFetchEvent();

  // Verify that the request body was passed to the fetch event.
  std::string body;
  fake_controller_.ReadRequestBody(&body);
  EXPECT_EQ(kData, body);
}

}  // namespace content
