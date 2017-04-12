// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_service.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

const char kTestServiceUrl[] = "system:content_test_service";

TestService::TestService() : service_binding_(this) {
  registry_.AddInterface<mojom::TestService>(this);
}

TestService::~TestService() {
}

void TestService::OnBindInterface(
    const service_manager::ServiceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  requestor_name_ = source_info.identity.name();
  registry_.BindInterface(source_info.identity, interface_name,
                          std::move(interface_pipe));
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
