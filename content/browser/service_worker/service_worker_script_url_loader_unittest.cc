// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_url_loader.h"

#include <map>
#include <utility>
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/common/resource_request_completion_status.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

const char kNormalScriptURL[] = "https://example.com/normal.js";

// MockHTTPServer is a utility to provide mocked responses for
// ServiceWorkerScriptURLLoader.
class MockHTTPServer {
 public:
  void AddResponse(const GURL& url,
                   const std::string& headers,
                   const std::string& body) {
    auto result =
        responses_.emplace(std::make_pair(url, MockResponse(headers, body)));
    ASSERT_TRUE(result.second);
  }

  const std::string& GetHeaders(const GURL& url) {
    auto found = responses_.find(url);
    if (found == responses_.end())
      return base::EmptyString();
    return found->second.headers;
  }

  const std::string& GetBody(const GURL& url) {
    auto found = responses_.find(url);
    if (found == responses_.end())
      return base::EmptyString();
    return found->second.body;
  }

 private:
  struct MockResponse {
    MockResponse(const std::string& headers, const std::string& body)
        : headers(headers), body(body) {}
    std::string headers;
    std::string body;
  };

  std::map<GURL, MockResponse> responses_;
};

// A URLLoaderFactory that returns a mocked response provided by MockHTTPServer.
//
// TODO(nhiroki): We copied this from service_worker_url_loader_job_unittest.cc
// instead of making it a common test helper because we might want to customize
// the mock factory to add more tests later. Merge this and that if we're
// convinced it's better.
class MockNetworkURLLoaderFactory final : public mojom::URLLoaderFactory {
 public:
  explicit MockNetworkURLLoaderFactory(MockHTTPServer* mock_server)
      : mock_server_(mock_server) {}

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    const std::string& headers = mock_server_->GetHeaders(url_request.url);
    net::HttpResponseInfo info;
    info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
    ResourceResponseHead response;
    response.headers = info.headers;
    response.headers->GetMimeType(&response.mime_type);
    client->OnReceiveResponse(response, base::nullopt, nullptr);

    // TODO(nhiroki): Add test cases where the body size is bigger than the
    // buffer size used in ServiceWorkerScriptURLLoader.
    const std::string& body = mock_server_->GetBody(url_request.url);
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
  // This is owned by ServiceWorkerScriptURLLoaderTest.
  MockHTTPServer* mock_server_;

  DISALLOW_COPY_AND_ASSIGN(MockNetworkURLLoaderFactory);
};

}  // namespace

// ServiceWorkerScriptURLLoaderTest is for testing the handling of requests for
// installing service worker scripts via ServiceWorkerScriptURLLoader.
class ServiceWorkerScriptURLLoaderTest : public testing::Test {
 public:
  ServiceWorkerScriptURLLoaderTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        mock_server_(std::make_unique<MockHTTPServer>()) {}
  ~ServiceWorkerScriptURLLoaderTest() override = default;

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(
        base::FilePath(), base::MakeRefCounted<URLLoaderFactoryGetter>());

    InitializeStorage();

    mock_server_->AddResponse(GURL(kNormalScriptURL),
                              std::string("HTTP/1.1 200 OK\n"
                                          "CONTENT-TYPE: text/javascript\n\n"),
                              std::string("this body came from the network"));

    // Initialize URLLoaderFactory.
    mojom::URLLoaderFactoryPtr test_loader_factory;
    mojo::MakeStrongBinding(
        std::make_unique<MockNetworkURLLoaderFactory>(mock_server_.get()),
        MakeRequest(&test_loader_factory));
    helper_->url_loader_factory_getter()->SetNetworkFactoryForTesting(
        std::move(test_loader_factory));
  }

  void InitializeStorage() {
    base::RunLoop run_loop;
    helper_->context()->storage()->LazyInitializeForTest(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Sets up ServiceWorkerRegistration and ServiceWorkerVersion. This should be
  // called before DoRequest().
  void SetUpRegistration(const GURL& script_url) {
    GURL scope = script_url;
    registration_ = base::MakeRefCounted<ServiceWorkerRegistration>(
        blink::mojom::ServiceWorkerRegistrationOptions(scope), 1L,
        helper_->context()->AsWeakPtr());
    version_ = base::MakeRefCounted<ServiceWorkerVersion>(
        registration_.get(), script_url, 1L, helper_->context()->AsWeakPtr());
    version_->SetStatus(ServiceWorkerVersion::NEW);
  }

  void DoRequest(const GURL& request_url) {
    DCHECK(registration_);
    DCHECK(version_);

    // Dummy values.
    int routing_id = 0;
    int request_id = 10;
    uint32_t options = 0;

    ResourceRequest request;
    request.url = version_->script_url();
    request.method = "GET";
    // TODO(nhiroki): Test importScripts() cases.
    request.resource_type = RESOURCE_TYPE_SERVICE_WORKER;

    DCHECK(!loader_);
    loader_ = std::make_unique<ServiceWorkerScriptURLLoader>(
        routing_id, request_id, options, request, client_.CreateInterfacePtr(),
        version_, helper_->url_loader_factory_getter(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  // Returns false if the entry for |url| doesn't exist in the storage.
  bool VerifyStoredResponse(const GURL& url) {
    int64_t cache_resource_id = LookupResourceId(url);
    if (cache_resource_id == kInvalidServiceWorkerResourceId)
      return false;

    // Verify the response status.
    {
      std::unique_ptr<ServiceWorkerResponseReader> reader =
          helper_->context()->storage()->CreateResponseReader(
              cache_resource_id);
      auto info_buffer = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
      net::TestCompletionCallback cb;
      reader->ReadInfo(info_buffer.get(), cb.callback());
      int rv = cb.WaitForResult();
      if (rv < 0)
        return false;
      EXPECT_LT(0, rv);
      EXPECT_EQ("OK", info_buffer->http_info->headers->GetStatusText());
    }

    // Verify the response body.
    {
      const std::string& expected_body = mock_server_->GetBody(url);
      std::unique_ptr<ServiceWorkerResponseReader> reader =
          helper_->context()->storage()->CreateResponseReader(
              cache_resource_id);
      auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(512);
      net::TestCompletionCallback cb;
      reader->ReadData(buffer.get(), buffer->size(), cb.callback());
      int rv = cb.WaitForResult();
      if (rv < 0)
        return false;
      EXPECT_EQ(static_cast<int>(expected_body.size()), rv);

      std::string received_body(buffer->data(), rv);
      EXPECT_EQ(expected_body, received_body);
    }
    return true;
  }

  int64_t LookupResourceId(const GURL& url) {
    return version_->script_cache_map()->LookupResourceId(url);
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  std::unique_ptr<ServiceWorkerScriptURLLoader> loader_;
  std::unique_ptr<MockHTTPServer> mock_server_;

  TestURLLoaderClient client_;
};

TEST_F(ServiceWorkerScriptURLLoaderTest, Success) {
  GURL script_url(kNormalScriptURL);
  SetUpRegistration(script_url);
  DoRequest(script_url);
  client_.RunUntilComplete();
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client_.has_received_response());
  EXPECT_TRUE(client_.response_body().is_valid());
  std::string response;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &response));
  EXPECT_EQ(mock_server_->GetBody(script_url), response);

  // The response should also be stored in the storage.
  EXPECT_TRUE(VerifyStoredResponse(script_url));
}

TEST_F(ServiceWorkerScriptURLLoaderTest, Success_EmptyBody) {
  const GURL kEmptyScriptURL("https://example.com/empty.js");
  mock_server_->AddResponse(kEmptyScriptURL,
                            std::string("HTTP/1.1 200 OK\n"
                                        "CONTENT-TYPE: text/javascript\n\n"),
                            std::string());
  SetUpRegistration(kEmptyScriptURL);
  DoRequest(kEmptyScriptURL);
  client_.RunUntilComplete();
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client_.has_received_response());
  EXPECT_TRUE(client_.response_body().is_valid());
  std::string response;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(
      client_.response_body_release(), &response));
  EXPECT_TRUE(response.empty());

  // The response should also be stored in the storage.
  EXPECT_TRUE(VerifyStoredResponse(kEmptyScriptURL));
}

TEST_F(ServiceWorkerScriptURLLoaderTest, Error_404) {
  const GURL kNonExistentScriptURL("https://example.com/nonexistent.js");
  mock_server_->AddResponse(kNonExistentScriptURL,
                            std::string("HTTP/1.1 404 Not Found\n\n"),
                            std::string());
  SetUpRegistration(kNonExistentScriptURL);
  DoRequest(kNonExistentScriptURL);
  client_.RunUntilComplete();

  // The request should be failed because of the 404 response.
  EXPECT_EQ(net::ERR_INVALID_RESPONSE, client_.completion_status().error_code);
  EXPECT_FALSE(client_.has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kNonExistentScriptURL));
}

TEST_F(ServiceWorkerScriptURLLoaderTest, Error_NoMimeType) {
  const GURL kNoMimeTypeScriptURL("https://example.com/no-mime-type.js");
  mock_server_->AddResponse(kNoMimeTypeScriptURL,
                            std::string("HTTP/1.1 200 OK\n\n"),
                            std::string("body with no MIME type"));
  SetUpRegistration(kNoMimeTypeScriptURL);
  DoRequest(kNoMimeTypeScriptURL);
  client_.RunUntilComplete();

  // The request should be failed because of the response with no MIME type.
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, client_.completion_status().error_code);
  EXPECT_FALSE(client_.has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kNoMimeTypeScriptURL));
}

TEST_F(ServiceWorkerScriptURLLoaderTest, Error_BadMimeType) {
  const GURL kBadMimeTypeScriptURL("https://example.com/bad-mime-type.js");
  mock_server_->AddResponse(kBadMimeTypeScriptURL,
                            std::string("HTTP/1.1 200 OK\n"
                                        "CONTENT-TYPE: text/css\n\n"),
                            std::string("body with bad MIME type"));
  SetUpRegistration(kBadMimeTypeScriptURL);
  DoRequest(kBadMimeTypeScriptURL);
  client_.RunUntilComplete();

  // The request should be failed because of the response with the bad MIME
  // type.
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, client_.completion_status().error_code);
  EXPECT_FALSE(client_.has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kBadMimeTypeScriptURL));
}

TEST_F(ServiceWorkerScriptURLLoaderTest, Error_RedundantWorker) {
  GURL script_url(kNormalScriptURL);
  SetUpRegistration(script_url);
  DoRequest(script_url);

  // Make the service worker redundant.
  version_->Doom();
  ASSERT_TRUE(version_->is_redundant());

  client_.RunUntilComplete();

  // The request should be aborted.
  EXPECT_EQ(net::ERR_FAILED, client_.completion_status().error_code);
  EXPECT_FALSE(client_.has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(script_url));
}

}  // namespace content
