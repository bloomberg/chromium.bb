// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/test/test_host_resolver.h"
#include "content/public/test/test_service.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/dns/mock_host_resolver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {

namespace {

class TestServiceImpl : public mojom::TestService {
 public:
  static void Create(const service_manager::BindSourceInfo&,
                     mojom::TestServiceRequest request) {
    mojo::MakeStrongBinding(base::WrapUnique(new TestServiceImpl),
                            std::move(request));
  }

  // mojom::TestService implementation:
  void DoSomething(DoSomethingCallback callback) override {
    std::move(callback).Run();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    base::Process::Current().Terminate(0, false);
  }

  void CreateFolder(CreateFolderCallback callback) override {
    // Note: This is used to check if the sandbox is disabled or not since
    //       creating a folder is forbidden when it is enabled.
    std::move(callback).Run(base::ScopedTempDir().CreateUniqueTempDir());
  }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    NOTREACHED();
  }

  void CreateSharedBuffer(const std::string& message,
                          CreateSharedBufferCallback callback) override {
    mojo::ScopedSharedBufferHandle buffer =
        mojo::SharedBufferHandle::Create(message.size());
    CHECK(buffer.is_valid());

    mojo::ScopedSharedBufferMapping mapping = buffer->Map(message.size());
    CHECK(mapping);
    std::copy(message.begin(), message.end(),
              reinterpret_cast<char*>(mapping.get()));

    std::move(callback).Run(std::move(buffer));
  }

 private:
  explicit TestServiceImpl() {}

  DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

std::unique_ptr<service_manager::Service> CreateTestService() {
  return std::unique_ptr<service_manager::Service>(new TestService);
}

class NetworkServiceTestImpl : public mojom::NetworkServiceTest {
 public:
  static void Create(mojom::NetworkServiceTestRequest request) {
    // Leak this.
    new NetworkServiceTestImpl(std::move(request));
  }
  explicit NetworkServiceTestImpl(mojom::NetworkServiceTestRequest request)
      : binding_(this, std::move(request)) {}
  ~NetworkServiceTestImpl() override = default;

  // mojom::NetworkServiceTest implementation.
  void AddRules(std::vector<mojom::RulePtr> rules) override {
    for (const auto& rule : rules) {
      test_host_resolver_.host_resolver()->AddRule(rule->host_pattern,
                                                   rule->replacement);
    }
  }

 private:
  mojo::Binding<mojom::NetworkServiceTest> binding_;

  TestHostResolver test_host_resolver_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestImpl);
};

}  // namespace

ShellContentUtilityClient::ShellContentUtilityClient() {}

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::UtilityThreadStarted() {
  auto registry = base::MakeUnique<service_manager::BinderRegistry>();
  registry->AddInterface(base::Bind(&TestServiceImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
  content::ChildThread::Get()
      ->GetServiceManagerConnection()
      ->AddConnectionFilter(
          base::MakeUnique<SimpleConnectionFilter>(std::move(registry)));
}

void ShellContentUtilityClient::RegisterServices(StaticServiceMap* services) {
  ServiceInfo info;
  info.factory = base::Bind(&CreateTestService);
  services->insert(std::make_pair(kTestServiceUrl, info));
}

void ShellContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface<mojom::NetworkServiceTest>(
      base::Bind(&ShellContentUtilityClient::BindNetworkServiceTestRequest,
                 base::Unretained(this)));
}

void ShellContentUtilityClient::BindNetworkServiceTestRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::NetworkServiceTestRequest request) {
  DCHECK(!network_service_test_);
  network_service_test_ =
      base::MakeUnique<NetworkServiceTestImpl>(std::move(request));
}

}  // namespace content
