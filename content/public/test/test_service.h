// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SERVICE_H_
#define CONTENT_PUBLIC_TEST_TEST_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace content {

extern const char kTestServiceUrl[];

// Simple Service which provides a mojom::TestService impl. The service
// terminates itself after its TestService fulfills a single DoSomething call.
class TestService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::TestService>,
      public mojom::TestService {
 public:
  TestService();
  ~TestService() override;

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // service_manager::InterfaceFactory<mojom::TestService>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::TestServiceRequest request) override;

  // TestService:
  void DoSomething(const DoSomethingCallback& callback) override;
  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override;
  void CreateFolder(const CreateFolderCallback& callback) override;
  void GetRequestorName(const GetRequestorNameCallback& callback) override;
  void CreateSharedBuffer(const std::string& message,
                          const CreateSharedBufferCallback& callback) override;

  mojo::Binding<mojom::TestService> service_binding_;

  // The name of the app connecting to us.
  std::string requestor_name_;

  DISALLOW_COPY_AND_ASSIGN(TestService);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SERVICE_H_
