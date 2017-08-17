// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
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
    mojom::NetworkServicePtr network_service;
    connector()->BindInterface(mojom::kNetworkServiceName, &network_service);

    mojom::NetworkContextPtr network_context;
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    network_service->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                          std::move(context_params));

    mojom::URLLoaderFactoryPtr loader_factory;
    network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                            0);

    mojom::URLLoaderPtr loader;
    ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 1, 1, mojom::kURLLoadOptionNone, request,
        client_.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    client_.RunUntilComplete();
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  TestURLLoaderClient* client() { return &client_; }

 private:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(this);
  }

  void SetUp() override {
    base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
    test_server_.AddDefaultHandlers(content_test_data);
    ASSERT_TRUE(test_server_.Start());
    service_manager::test::ServiceTest::SetUp();
  }

  net::EmbeddedTestServer test_server_;
  TestURLLoaderClient client_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestWithService);
};

// Verifies that loading a URL through the network service's mojo interface
// works.
TEST_F(NetworkServiceTestWithService, Basic) {
  LoadURL(test_server()->GetURL("/echo"));
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

}  // namespace

}  // namespace content
