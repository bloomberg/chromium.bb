// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_subresource_loader.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/child/child_url_loader_factory_getter_impl.h"
#include "content/child/service_worker/controller_service_worker_connector.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// This class need to set ChildURLLoaderFactoryGetter. CreateLoaderAndStart()
// need to implement. todo(emim): Merge this and the one in
// service_worker_url_loader_job_unittest.cc.
class FakeNetworkURLLoaderFactory final : public mojom::URLLoaderFactory {
 public:
  FakeNetworkURLLoaderFactory() = default;

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
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkURLLoaderFactory);
};

class FakeControllerServiceWorker : public mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;
  ~FakeControllerServiceWorker() override = default;

  void AddBinding(mojom::ControllerServiceWorkerRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void CloseAllBindings() { bindings_.CloseAllBindings(); }

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

  // mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      const ServiceWorkerFetchRequest& request,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    fetch_event_count_++;
    fetch_event_request_ = request;
    switch (response_mode_) {
      case ResponseMode::kDefault:
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED, base::Time());
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
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED, base::Time());
        return;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(base::Time::Now());
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED,
            base::Time::Now());
        return;
      case ResponseMode::kErrorResponse:
        response_callback->OnResponse(
            ServiceWorkerResponse(
                base::MakeUnique<std::vector<GURL>>(), 0 /* status_code */,
                "" /* status_text */,
                network::mojom::FetchResponseType::kDefault,
                base::MakeUnique<ServiceWorkerHeaderMap>(), "" /* blob_uuid */,
                0 /* blob_size */, nullptr /* blob */,
                blink::kWebServiceWorkerResponseErrorPromiseRejected,
                base::Time(), false /* response_is_in_cache_storage */,
                std::string() /* response_cache_storage_cache_name */,
                base::MakeUnique<
                    ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
            base::Time::Now());
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::REJECTED,
            base::Time::Now());
        return;
    }
    NOTREACHED();
  }

  int fetch_event_count() const { return fetch_event_count_; }
  const ServiceWorkerFetchRequest& fetch_event_request() const {
    return fetch_event_request_;
  }

 private:
  enum class ResponseMode {
    kDefault,
    kStream,
    kFallbackResponse,
    kErrorResponse
  };

  ResponseMode response_mode_ = ResponseMode::kDefault;

  int fetch_event_count_ = 0;
  ServiceWorkerFetchRequest fetch_event_request_;
  mojo::BindingSet<mojom::ControllerServiceWorker> bindings_;
  // For ResponseMode::kStream.
  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

  DISALLOW_COPY_AND_ASSIGN(FakeControllerServiceWorker);
};

class FakeServiceWorkerContainerHost
    : public mojom::ServiceWorkerContainerHost {
 public:
  explicit FakeServiceWorkerContainerHost(
      FakeControllerServiceWorker* fake_controller)
      : fake_controller_(fake_controller) {}

  ~FakeServiceWorkerContainerHost() override = default;

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
    fake_controller_->AddBinding(std::move(request));
  }

  int get_controller_service_worker_count_ = 0;
  FakeControllerServiceWorker* fake_controller_;
  DISALLOW_COPY_AND_ASSIGN(FakeServiceWorkerContainerHost);
};

}  // namespace

// Returns typical response info for a resource load that went through a service
// worker.
std::unique_ptr<ResourceResponseHead> CreateResponseInfoFromServiceWorker() {
  auto head = std::make_unique<ResourceResponseHead>();
  head->was_fetched_via_service_worker = true;
  head->was_fallback_required_by_service_worker = false;
  head->url_list_via_service_worker = std::vector<GURL>();
  head->response_type_via_service_worker =
      network::mojom::FetchResponseType::kDefault;
  // TODO(emim): start and ready time should be set.
  head->service_worker_start_time = base::TimeTicks();
  head->service_worker_ready_time = base::TimeTicks();
  head->is_in_cache_storage = false;
  head->cache_storage_cache_name = std::string();
  head->did_service_worker_navigation_preload = false;
  return head;
}

class ServiceWorkerSubresourceLoaderTest : public ::testing::Test {
 protected:
  ServiceWorkerSubresourceLoaderTest()
      : fake_container_host_(&fake_controller_),
        url_loader_client_(base::MakeUnique<TestURLLoaderClient>()) {}
  ~ServiceWorkerSubresourceLoaderTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kNetworkService);

    mojom::URLLoaderFactoryPtr fake_loader_factory;
    mojo::MakeStrongBinding(base::MakeUnique<FakeNetworkURLLoaderFactory>(),
                            MakeRequest(&fake_loader_factory));
    loader_factory_getter_ =
        base::MakeRefCounted<ChildURLLoaderFactoryGetterImpl>(
            std::move(fake_loader_factory), nullptr);
    controller_connector_ =
        base::MakeRefCounted<ControllerServiceWorkerConnector>(
            &fake_container_host_);
  }

  void TearDown() override {
    controller_connector_->OnContainerHostConnectionClosed();
  }

  void TestRequest(std::unique_ptr<ResourceRequest> request) {
    mojom::URLLoaderPtr url_loader;

    ServiceWorkerSubresourceLoaderFactory loader_factory(
        controller_connector_, loader_factory_getter_, request->url.GetOrigin(),
        base::MakeRefCounted<
            base::RefCountedData<storage::mojom::BlobRegistryPtr>>());
    loader_factory.CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader), 0, 0, mojom::kURLLoadOptionNone,
        *request, url_loader_client_->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(request->url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request->method, fake_controller_.fetch_event_request().method);
  }

  std::unique_ptr<ResourceRequest> CreateRequest(const GURL& url) {
    std::unique_ptr<ResourceRequest> request =
        base::MakeUnique<ResourceRequest>();
    request->url = url;
    request->method = "GET";
    return request;
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
    EXPECT_EQ(expected_info.service_worker_start_time,
              info.service_worker_start_time);
    EXPECT_EQ(expected_info.service_worker_ready_time,
              info.service_worker_ready_time);
    EXPECT_EQ(expected_info.is_in_cache_storage, info.is_in_cache_storage);
    EXPECT_EQ(expected_info.cache_storage_cache_name,
              info.cache_storage_cache_name);
    EXPECT_EQ(expected_info.did_service_worker_navigation_preload,
              info.did_service_worker_navigation_preload);
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<ChildURLLoaderFactoryGetter> loader_factory_getter_;

  FakeServiceWorkerContainerHost fake_container_host_;
  FakeControllerServiceWorker fake_controller_;
  scoped_refptr<ControllerServiceWorkerConnector> controller_connector_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<TestURLLoaderClient> url_loader_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderTest);
};

TEST_F(ServiceWorkerSubresourceLoaderTest, Basic) {
  TestRequest(CreateRequest(GURL("https://www.example.com/foo.html")));
  EXPECT_EQ(1, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController) {
  TestRequest(CreateRequest(GURL("https://www.example.com/foo.html")));
  url_loader_client_->Unbind();
  EXPECT_EQ(1, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  TestRequest(CreateRequest(GURL("https://www.example.com/foo2.html")));
  url_loader_client_->Unbind();
  EXPECT_EQ(2, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.CloseAllBindings();
  base::RunLoop().RunUntilIdle();

  // This should re-obtain the ControllerServiceWorker.
  TestRequest(CreateRequest(GURL("https://www.example.com/foo3.html")));
  EXPECT_EQ(3, fake_controller_.fetch_event_count());
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
}

TEST_F(ServiceWorkerSubresourceLoaderTest, StreamResponse) {
  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  fake_controller_.RespondWithStream(mojo::MakeRequest(&stream_callback),
                                     std::move(data_pipe.consumer_handle));

  // Perform the request.
  TestRequest(CreateRequest(GURL("https://www.example.com/foo.html")));

  const ResourceResponseHead& info = url_loader_client_->response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Write the body stream.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();

  url_loader_client_->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client_->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(url_loader_client_->response_body().is_valid());
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      url_loader_client_->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerSubresourceLoaderTest, FallbackResponse) {
  fake_controller_.RespondWithFallback();

  // Perform the request.
  TestRequest(CreateRequest(GURL("https://www.example.com/foo.html")));
  // TODO(emim): It should add some expression to check whether this actually
  // performed the fallback.
}

TEST_F(ServiceWorkerSubresourceLoaderTest, ErrorResponse) {
  fake_controller_.RespondWithError();

  // Perform the request.
  TestRequest(CreateRequest(GURL("https://www.example.com/foo.html")));

  EXPECT_EQ(net::ERR_FAILED,
            url_loader_client_->completion_status().error_code);
}

// Test when the service worker responds with network fallback to CORS request.
TEST_F(ServiceWorkerSubresourceLoaderTest, CORSFallbackResponse) {
  fake_controller_.RespondWithFallback();

  struct TestCase {
    FetchRequestMode fetch_request_mode;
    base::Optional<url::Origin> request_initiator;
    bool expected_was_fallback_required_by_service_worker;
  };
  const TestCase kTests[] = {
      {FETCH_REQUEST_MODE_SAME_ORIGIN, base::Optional<url::Origin>(), false},
      {FETCH_REQUEST_MODE_NO_CORS, base::Optional<url::Origin>(), false},
      {FETCH_REQUEST_MODE_CORS, base::Optional<url::Origin>(), true},
      {FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
       base::Optional<url::Origin>(), true},
      {FETCH_REQUEST_MODE_NAVIGATE, base::Optional<url::Origin>(), false},
      {FETCH_REQUEST_MODE_SAME_ORIGIN,
       url::Origin(GURL("https://www.example.com/")), false},
      {FETCH_REQUEST_MODE_NO_CORS,
       url::Origin(GURL("https://www.example.com/")), false},
      {FETCH_REQUEST_MODE_CORS, url::Origin(GURL("https://www.example.com/")),
       false},
      {FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
       url::Origin(GURL("https://www.example.com/")), false},
      {FETCH_REQUEST_MODE_NAVIGATE,
       url::Origin(GURL("https://other.example.com/")), false},
      {FETCH_REQUEST_MODE_SAME_ORIGIN,
       url::Origin(GURL("https://other.example.com/")), false},
      {FETCH_REQUEST_MODE_NO_CORS,
       url::Origin(GURL("https://other.example.com/")), false},
      {FETCH_REQUEST_MODE_CORS, url::Origin(GURL("https://other.example.com/")),
       true},
      {FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
       url::Origin(GURL("https://other.example.com/")), true},
      {FETCH_REQUEST_MODE_NAVIGATE,
       url::Origin(GURL("https://other.example.com/")), false}};

  for (const auto& test : kTests) {
    SCOPED_TRACE(
        ::testing::Message()
        << "fetch_request_mode: " << static_cast<int>(test.fetch_request_mode)
        << " request_initiator: "
        << (test.request_initiator ? test.request_initiator->Serialize()
                                   : std::string("null")));
    std::unique_ptr<ResourceRequest> request =
        CreateRequest(GURL("https://www.example.com/foo.html"));
    request->fetch_request_mode = test.fetch_request_mode;
    request->request_initiator = test.request_initiator;

    // Perform the request.
    TestRequest(std::move(request));

    const ResourceResponseHead& info = url_loader_client_->response_head();
    EXPECT_EQ(test.expected_was_fallback_required_by_service_worker,
              info.was_fetched_via_service_worker);
    EXPECT_EQ(test.expected_was_fallback_required_by_service_worker,
              info.was_fallback_required_by_service_worker);
    url_loader_client_ = base::MakeUnique<TestURLLoaderClient>();
  }
}

// TODO(kinuko): Add more tests.

}  // namespace content
