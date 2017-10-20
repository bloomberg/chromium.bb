// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/loader/cors_url_loader.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/child/loader/cors_url_loader_factory.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/test_url_loader_client.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() = default;
  ~TestURLLoaderFactory() override = default;

  void NotifyClientOnReceiveResponse() {
    DCHECK(client_ptr_);
    ResourceResponseHead response;
    response.headers = new net::HttpResponseHeaders(
        "HTTP/1.1 200 OK\n"
        "Content-Type: text/html; charset=utf-8\n");

    client_ptr_->OnReceiveResponse(response, base::nullopt /* ssl_info */,
                                   nullptr /* downloaded_file */);
  }

  void NotifyClientOnComplete(int error_code) {
    DCHECK(client_ptr_);
    client_ptr_->OnComplete(ResourceRequestCompletionStatus(error_code));
  }

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
  void CreateLoaderAndStart(const GURL& origin, const GURL& url) {
    mojom::URLLoaderFactoryPtr network_factory_ptr;
    test_network_factory_binding_.Bind(mojo::MakeRequest(&network_factory_ptr));

    mojom::URLLoaderFactoryPtr loader_factory_ptr;
    CORSURLLoaderFactory::CreateAndBind(network_factory_ptr.PassInterface(),
                                        mojo::MakeRequest(&loader_factory_ptr));

    ResourceRequest request;
    request.resource_type = RESOURCE_TYPE_IMAGE;
    request.fetch_request_context_type = REQUEST_CONTEXT_TYPE_IMAGE;
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

  void NotifyLoaderClientOnReceiveResponse() {
    test_network_loader_factory_.NotifyClientOnReceiveResponse();
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
  CreateLoaderAndStart(url.GetOrigin(), url);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

}  // namespace
}  // namespace content
