// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_mojo_app.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"

namespace content {

const char kTestMojoAppUrl[] = "system:content_mojo_test";

TestMojoApp::TestMojoApp() : service_binding_(this) {
}

TestMojoApp::~TestMojoApp() {
}

bool TestMojoApp::AcceptConnection(shell::Connection* connection) {
  requestor_name_ = connection->GetRemoteIdentity().name();
  connection->AddInterface<mojom::TestMojoService>(this);
  return true;
}

void TestMojoApp::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<mojom::TestMojoService> request) {
  DCHECK(!service_binding_.is_bound());
  service_binding_.Bind(std::move(request));
}

void TestMojoApp::DoSomething(const DoSomethingCallback& callback) {
  callback.Run();
  base::MessageLoop::current()->QuitWhenIdle();
}

void TestMojoApp::DoTerminateProcess(
    const DoTerminateProcessCallback& callback) {
  NOTREACHED();
}

void TestMojoApp::CreateFolder(const CreateFolderCallback& callback) {
  NOTREACHED();
}

void TestMojoApp::GetRequestorName(const GetRequestorNameCallback& callback) {
  callback.Run(requestor_name_);
}

}  // namespace content
