// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/network/network_context.h"
#include "content/network/network_service_impl.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/test_url_loader_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class NetworkServiceTest : public testing::Test {
 public:
  NetworkServiceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        service_(NetworkServiceImpl::CreateForTesting()) {}
  ~NetworkServiceTest() override {}

  NetworkService* service() const { return service_.get(); }

  void DestroyService() { service_.reset(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkService> service_;
};

// Test shutdown in the case a NetworkContext is destroyed before the
// NetworkService.
TEST_F(NetworkServiceTest, CreateAndDestroyContext) {
  mojom::NetworkContextPtr network_context;
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();

  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  std::move(context_params));
  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

// Test shutdown in the case there is still a live NetworkContext when the
// NetworkService is destroyed. The service should destroy the NetworkContext
// itself.
TEST_F(NetworkServiceTest, DestroyingServiceDestroysContext) {
  mojom::NetworkContextPtr network_context;
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();

  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  std::move(context_params));
  base::RunLoop run_loop;
  network_context.set_connection_error_handler(run_loop.QuitClosure());
  DestroyService();

  // Destroying the service should destroy the context, causing a connection
  // error.
  run_loop.Run();
}

namespace {

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  explicit ServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(base::Bind(
        &ServiceTestClient::BindServiceFactoryRequest, base::Unretained(this)));
  }
  ~ServiceTestClient() override {}

 protected:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == mojom::kNetworkServiceName) {
      service_context_.reset(new service_manager::ServiceContext(
          NetworkServiceImpl::CreateForTesting(), std::move(request)));
    }
  }

  void BindServiceFactoryRequest(
      service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> service_context_;
};

}  // namespace

class NetworkServiceTestWithService
    : public service_manager::test::ServiceTest {
 public:
  NetworkServiceTestWithService()
      : ServiceTest("content_unittests",
                    false,
                    base::test::ScopedTaskEnvironment::MainThreadType::IO) {}
  ~NetworkServiceTestWithService() override {}

  void LoadURL(const GURL& url) {
    ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();
    StartLoadingURL(request, 0);
    client_->RunUntilComplete();
  }

  void StartLoadingURL(const ResourceRequest& request, uint32_t process_id) {
    client_.reset(new TestURLLoaderClient());
    mojom::URLLoaderFactoryPtr loader_factory;
    network_context_->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                             process_id);

    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader_), 1, 1, mojom::kURLLoadOptionNone, request,
        client_->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  TestURLLoaderClient* client() { return client_.get(); }
  mojom::URLLoader* loader() { return loader_.get(); }
  mojom::NetworkService* service() { return network_service_.get(); }
  mojom::NetworkContext* context() { return network_context_.get(); }

 private:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(this);
  }

  void SetUp() override {
    base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
    test_server_.AddDefaultHandlers(content_test_data);
    ASSERT_TRUE(test_server_.Start());
    service_manager::test::ServiceTest::SetUp();
    connector()->BindInterface(mojom::kNetworkServiceName, &network_service_);
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    network_service_->CreateNetworkContext(mojo::MakeRequest(&network_context_),
                                           std::move(context_params));
  }

  net::EmbeddedTestServer test_server_;
  std::unique_ptr<TestURLLoaderClient> client_;
  mojom::NetworkServicePtr network_service_;
  mojom::NetworkContextPtr network_context_;
  mojom::URLLoaderPtr loader_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestWithService);
};

// Verifies that loading a URL through the network service's mojo interface
// works.
TEST_F(NetworkServiceTestWithService, Basic) {
  LoadURL(test_server()->GetURL("/echo"));
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

// Verifies that raw headers are only reported if requested.
TEST_F(NetworkServiceTestWithService, RawRequestHeadersAbsent) {
  ResourceRequest request;
  request.url = test_server()->GetURL("/server-redirect?/echo");
  request.method = "GET";
  request.request_initiator = url::Origin();
  StartLoadingURL(request, 0);
  client()->RunUntilRedirectReceived();
  EXPECT_TRUE(client()->has_received_redirect());
  EXPECT_TRUE(!client()->response_head().devtools_info);
  loader()->FollowRedirect();
  client()->RunUntilComplete();
  EXPECT_TRUE(!client()->response_head().devtools_info);
}

TEST_F(NetworkServiceTestWithService, RawRequestHeadersPresent) {
  ResourceRequest request;
  request.url = test_server()->GetURL("/server-redirect?/echo");
  request.method = "GET";
  request.report_raw_headers = true;
  request.request_initiator = url::Origin();
  StartLoadingURL(request, 0);
  client()->RunUntilRedirectReceived();
  EXPECT_TRUE(client()->has_received_redirect());
  {
    scoped_refptr<ResourceDevToolsInfo> devtools_info =
        client()->response_head().devtools_info;
    ASSERT_TRUE(devtools_info);
    EXPECT_EQ(301, devtools_info->http_status_code);
    EXPECT_EQ("Moved Permanently", devtools_info->http_status_text);
    EXPECT_TRUE(base::StartsWith(devtools_info->request_headers_text,
                                 "GET /server-redirect?/echo HTTP/1.1\r\n",
                                 base::CompareCase::SENSITIVE));
    EXPECT_GE(devtools_info->request_headers.size(), 1lu);
    EXPECT_GE(devtools_info->response_headers.size(), 1lu);
    EXPECT_TRUE(base::StartsWith(devtools_info->response_headers_text,
                                 "HTTP/1.1 301 Moved Permanently\r",
                                 base::CompareCase::SENSITIVE));
  }
  loader()->FollowRedirect();
  client()->RunUntilComplete();
  {
    scoped_refptr<ResourceDevToolsInfo> devtools_info =
        client()->response_head().devtools_info;
    EXPECT_EQ(200, devtools_info->http_status_code);
    EXPECT_EQ("OK", devtools_info->http_status_text);
    EXPECT_TRUE(base::StartsWith(devtools_info->request_headers_text,
                                 "GET /echo HTTP/1.1\r\n",
                                 base::CompareCase::SENSITIVE));
    EXPECT_GE(devtools_info->request_headers.size(), 1lu);
    EXPECT_GE(devtools_info->response_headers.size(), 1lu);
    EXPECT_TRUE(base::StartsWith(devtools_info->response_headers_text,
                                 "HTTP/1.1 200 OK\r",
                                 base::CompareCase::SENSITIVE));
  }
}

TEST_F(NetworkServiceTestWithService, RawRequestAccessControl) {
  const uint32_t process_id = 42;
  ResourceRequest request;
  request.url = test_server()->GetURL("/nocache.html");
  request.method = "GET";
  request.report_raw_headers = true;
  request.request_initiator = url::Origin();

  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  EXPECT_FALSE(client()->response_head().devtools_info);
  service()->SetRawHeadersAccess(process_id, true);
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  {
    scoped_refptr<ResourceDevToolsInfo> devtools_info =
        client()->response_head().devtools_info;
    ASSERT_TRUE(devtools_info);
    EXPECT_EQ(200, devtools_info->http_status_code);
    EXPECT_EQ("OK", devtools_info->http_status_text);
  }

  service()->SetRawHeadersAccess(process_id, false);
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  EXPECT_FALSE(client()->response_head().devtools_info.get());
}

TEST_F(NetworkServiceTestWithService, SetNetworkConditions) {
  mojom::NetworkConditionsPtr network_conditions =
      mojom::NetworkConditions::New();
  network_conditions->offline = true;
  context()->SetNetworkConditions("42", std::move(network_conditions));

  ResourceRequest request;
  request.url = test_server()->GetURL("/nocache.html");
  request.method = "GET";

  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  request.headers.AddHeaderFromString(
      "X-DevTools-Emulate-Network-Conditions-Client-Id: 42");
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_INTERNET_DISCONNECTED,
            client()->completion_status().error_code);

  network_conditions = mojom::NetworkConditions::New();
  network_conditions->offline = false;
  context()->SetNetworkConditions("42", std::move(network_conditions));
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  network_conditions = mojom::NetworkConditions::New();
  network_conditions->offline = true;
  context()->SetNetworkConditions("42", std::move(network_conditions));

  request.headers.AddHeaderFromString(
      "X-DevTools-Emulate-Network-Conditions-Client-Id: 42");
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_INTERNET_DISCONNECTED,
            client()->completion_status().error_code);
  context()->SetNetworkConditions("42", nullptr);
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

}  // namespace

}  // namespace content
