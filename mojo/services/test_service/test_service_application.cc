// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_application.h"

#include <assert.h>
#include <utility>

#include "mojo/public/c/system/main.h"
#include "mojo/services/test_service/test_service_impl.h"
#include "mojo/services/test_service/test_time_service_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/connection.h"

namespace mojo {
namespace test {

TestServiceApplication::TestServiceApplication()
    : ref_count_(0), shell_(nullptr) {
}

TestServiceApplication::~TestServiceApplication() {
}

void TestServiceApplication::Initialize(Shell* shell, const std::string& url,
                                        uint32_t id) {
  shell_ = shell;
}

bool TestServiceApplication::AcceptConnection(Connection* connection) {
  connection->AddService<TestService>(this);
  connection->AddService<TestTimeService>(this);
  return true;
}

void TestServiceApplication::Create(Connection* connection,
                                    InterfaceRequest<TestService> request) {
  new TestServiceImpl(shell_, this, std::move(request));
  AddRef();
}

void TestServiceApplication::Create(Connection* connection,
                                    InterfaceRequest<TestTimeService> request) {
  new TestTimeServiceImpl(shell_, std::move(request));
}

void TestServiceApplication::AddRef() {
  assert(ref_count_ >= 0);
  ref_count_++;
}

void TestServiceApplication::ReleaseRef() {
  assert(ref_count_ > 0);
  ref_count_--;
  if (ref_count_ <= 0)
    base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace test
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::test::TestServiceApplication);
  return runner.Run(shell_handle);
}
