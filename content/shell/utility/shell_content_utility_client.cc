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
#include "content/public/test/test_service.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace content {

namespace {

class TestServiceImpl : public mojom::TestService {
 public:
  static void Create(mojom::TestServiceRequest request) {
    mojo::MakeStrongBinding(base::WrapUnique(new TestServiceImpl),
                            std::move(request));
  }

  // mojom::TestService implementation:
  void DoSomething(const DoSomethingCallback& callback) override {
    callback.Run();
  }

  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override {
    base::Process::Current().Terminate(0, false);
  }

  void CreateFolder(const CreateFolderCallback& callback) override {
    // Note: This is used to check if the sandbox is disabled or not since
    //       creating a folder is forbidden when it is enabled.
    callback.Run(base::ScopedTempDir().CreateUniqueTempDir());
  }

  void GetRequestorName(const GetRequestorNameCallback& callback) override {
    NOTREACHED();
  }

  void CreateSharedBuffer(const std::string& message,
                          const CreateSharedBufferCallback& callback) override {
    mojo::ScopedSharedBufferHandle buffer =
        mojo::SharedBufferHandle::Create(message.size());
    CHECK(buffer.is_valid());

    mojo::ScopedSharedBufferMapping mapping = buffer->Map(message.size());
    CHECK(mapping);
    std::copy(message.begin(), message.end(),
              reinterpret_cast<char*>(mapping.get()));

    callback.Run(std::move(buffer));
  }

 private:
  explicit TestServiceImpl() {}

  DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

std::unique_ptr<service_manager::Service> CreateTestService() {
  return std::unique_ptr<service_manager::Service>(new TestService);
}

}  // namespace

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::RegisterServices(StaticServiceMap* services) {
  ServiceInfo info;
  info.factory = base::Bind(&CreateTestService);
  services->insert(std::make_pair(kTestServiceUrl, info));
}

void ShellContentUtilityClient::ExposeInterfacesToBrowser(
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface(base::Bind(&TestServiceImpl::Create));
}

}  // namespace content
