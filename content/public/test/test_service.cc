// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_service.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace content {

const char kTestServiceUrl[] = "system:content_test_service";

TestService::TestService() : service_binding_(this) {
}

TestService::~TestService() {
}

bool TestService::OnConnect(const service_manager::ServiceInfo& remote_info,
                            service_manager::InterfaceRegistry* registry) {
  requestor_name_ = remote_info.identity.name();
  registry->AddInterface<mojom::TestService>(this);
  return true;
}

void TestService::Create(const service_manager::Identity& remote_identity,
                         mojom::TestServiceRequest request) {
  DCHECK(!service_binding_.is_bound());
  service_binding_.Bind(std::move(request));
}

void TestService::DoSomething(const DoSomethingCallback& callback) {
  callback.Run();
  base::MessageLoop::current()->QuitWhenIdle();
}

void TestService::DoTerminateProcess(
    const DoTerminateProcessCallback& callback) {
  NOTREACHED();
}

void TestService::CreateFolder(const CreateFolderCallback& callback) {
  NOTREACHED();
}

void TestService::GetRequestorName(const GetRequestorNameCallback& callback) {
  callback.Run(requestor_name_);
}

void TestService::CreateSharedBuffer(
    const std::string& message,
    const CreateSharedBufferCallback& callback) {
  NOTREACHED();
}

}  // namespace content
