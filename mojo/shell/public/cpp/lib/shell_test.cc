// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/shell_test.h"

#include "base/message_loop/message_loop.h"
#include "mojo/shell/background/background_shell.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
namespace test {

ShellTestClient::ShellTestClient(ShellTest* test) : test_(test) {}
ShellTestClient::~ShellTestClient() {}

void ShellTestClient::Initialize(Connector* connector, const std::string& name,
                                 uint32_t id, uint32_t user_id) {
  test_->InitializeCalled(connector, name, id, user_id);
}

ShellTest::ShellTest() {}
ShellTest::ShellTest(const std::string& test_name) : test_name_(test_name) {}
ShellTest::~ShellTest() {}

void ShellTest::InitTestName(const std::string& test_name) {
  DCHECK(test_name_.empty());
  test_name_ = test_name;
}

scoped_ptr<ShellClient> ShellTest::CreateShellClient() {
  return make_scoped_ptr(new ShellTestClient(this));
}

void ShellTest::InitializeCalled(Connector* connector,
                                 const std::string& name,
                                 uint32_t id,
                                 uint32_t userid) {
  connector_ = connector;
  initialize_name_ = name;
  initialize_instance_id_ = id;
  initialize_userid_ = userid;
}

void ShellTest::SetUp() {
  shell_client_ = CreateShellClient();
  message_loop_.reset(new base::MessageLoop);
  background_shell_.reset(new shell::BackgroundShell);
  background_shell_->Init(nullptr);
  shell_connection_.reset(new ShellConnection(
      shell_client_.get(),
      background_shell_->CreateShellClientRequest(test_name_)));
  shell_connection_->WaitForInitialize();
}

void ShellTest::TearDown() {
  shell_connection_.reset();
  background_shell_.reset();
  message_loop_.reset();
  shell_client_.reset();
}

}  // namespace test
}  // namespace mojo
