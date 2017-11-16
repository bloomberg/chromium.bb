// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/cors_url_loader.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/test_url_loader_client.h"
#include "content/renderer/loader/cors_url_loader_factory.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using network::mojom::FetchRequestMode;

namespace content {
namespace {

class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() = default;
  ~TestURLLoaderFactory() override = default;

  void NotifyClientOnReceiveResponse(const std::string& extra_header) {
    DCHECK(client_ptr_);
    ResourceResponseHead response;
    response.headers = new net::HttpResponseHeaders(
        "HTTP/1.1 200 OK\n"
        "Content-Type: image/png\n");
    if (!extra_header.empty())
      response.headers->AddHeader(extra_header);

    client_ptr_->OnReceiveResponse(response, base::nullopt /* ssl_info */,
                                   nullptr /* downloaded_file */);
  }

  void NotifyClientOnComplete(int error_code) {
    DCHECK(client_ptr_);
    client_ptr_->OnComplete(network::URLLoaderStatus(error_code));
  }

  bool IsCreateLoaderAndStartCalled() { return !!client_ptr_; }

 private:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK(client);
    client_ptr_ = std::move(client);
  }

  void Clone(mojom::URLLoaderFactoryRequest request) override { NOTREACHED(); }

  mojom::URLLoaderClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class CORSURLLoaderTest : public testing::Test {
 public:
  CORSURLLoaderTest()
      : test_network_factory_binding_(&test_network_loader_factory_) {}

 protected:
  void CreateLoaderAndStart(const GURL& origin,
                            const GURL& url,
                            FetchRequestMode fetch_request_mode) {
    mojom::URLLoaderFactoryPtr network_factory_ptr;
    test_network_factory_binding_.Bind(mojo::MakeRequest(&network_factory_ptr));

    mojom::URLLoaderFactoryPtr loader_factory_ptr;
    CORSURLLoaderFactory::CreateAndBind(network_factory_ptr.PassInterface(),
                                        mojo::MakeRequest(&loader_factory_ptr));

    ResourceRequest request;
    request.resource_type = RESOURCE_TYPE_IMAGE;
    request.fetch_request_context_type = REQUEST_CONTEXT_TYPE_IMAGE;
    request.fetch_request_mode = fetch_request_mode;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = url;
    request.request_initiator = url::Origin::Create(origin);

    loader_factory_ptr->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionNone, request,
        test_cors_loader_client_.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    // Flushes to ensure that the request is handled and
    // TestURLLoaderFactory::CreateLoaderAndStart() runs internally.
    loader_factory_ptr.FlushForTesting();
  }

  bool IsNetworkLoaderStarted() {
    return test_network_loader_factory_.IsCreateLoaderAndStartCalled();
  }

  void NotifyLoaderClientOnReceiveResponse(
      const std::string& extra_header = std::string()) {
    test_network_loader_factory_.NotifyClientOnReceiveResponse(extra_header);
  }

  void NotifyLoaderClientOnComplete(int error_code) {
    test_network_loader_factory_.NotifyClientOnComplete(error_code);
  }

  const TestURLLoaderClient& client() const { return test_cors_loader_client_; }

  void RunUntilComplete() { test_cors_loader_client_.RunUntilComplete(); }

 private:
  // Be the first member so it is destroyed last.
  base::MessageLoop message_loop_;

  // TestURLLoaderFactory instance and mojo binding.
  TestURLLoaderFactory test_network_loader_factory_;
  mojo::Binding<mojom::URLLoaderFactory> test_network_factory_binding_;

  // Holds URLLoaderPtr that CreateLoaderAndStart() creates.
  mojom::URLLoaderPtr url_loader_;

  // TestURLLoaderClient that records callback activities.
  TestURLLoaderClient test_cors_loader_client_;

  DISALLOW_COPY_AND_ASSIGN(CORSURLLoaderTest);
};

TEST_F(CORSURLLoaderTest, SameOriginRequest) {
  const GURL url("http://example.com/foo.png");
  CreateLoaderAndStart(url.GetOrigin(), url, FetchRequestMode::kSameOrigin);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().status().error_code);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithNoCORSMode) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, FetchRequestMode::kNoCORS);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().status().error_code);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestFetchRequestModeSameOrigin) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url,
                       network::mojom::FetchRequestMode::kSameOrigin);

  RunUntilComplete();

  // This call never hits the network URLLoader (i.e. the TestURLLoaderFactory)
  // because it is fails right away.
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().status().error_code);
  ASSERT_TRUE(client().status().cors_error);
  EXPECT_EQ(network::mojom::CORSError::kDisallowedByMode,
            *client().status().cors_error);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithCORSModeButMissingCORSHeader) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().status().error_code);
  ASSERT_TRUE(client().status().cors_error);
  EXPECT_EQ(network::mojom::CORSError::kMissingAllowOriginHeader,
            *client().status().cors_error);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithCORSMode) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse(
      "Access-Control-Allow-Origin: http://example.com");
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().status().error_code);
}

TEST_F(CORSURLLoaderTest,
       CrossOriginRequestFetchRequestWithCORSModeButMismatchedCORSHeader) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse(
      "Access-Control-Allow-Origin: http://some-other-domain.com");
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().status().error_code);
  ASSERT_TRUE(client().status().cors_error);
  EXPECT_EQ(network::mojom::CORSError::kAllowOriginMismatch,
            *client().status().cors_error);
}

}  // namespace
}  // namespace content
