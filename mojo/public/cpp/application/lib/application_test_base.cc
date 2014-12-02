// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_test_base.h"

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace test {

namespace {

// This shell handle is shared by multiple test application instances.
MessagePipeHandle g_shell_handle;
// Share the application command-line arguments with multiple application tests.
Array<String> g_args;

}  // namespace

ScopedMessagePipeHandle PassShellHandle() {
  MOJO_CHECK(g_shell_handle.is_valid());
  ScopedMessagePipeHandle scoped_handle(g_shell_handle);
  g_shell_handle = MessagePipeHandle();
  return scoped_handle.Pass();
}

void SetShellHandle(ScopedMessagePipeHandle handle) {
  MOJO_CHECK(handle.is_valid());
  MOJO_CHECK(!g_shell_handle.is_valid());
  g_shell_handle = handle.release();
}

const Array<String>& Args() {
  return g_args;
}

void InitializeArgs(int argc, std::vector<const char*> argv) {
  MOJO_CHECK(g_args.is_null());
  for (const char* arg : argv) {
    if (arg)
      g_args.push_back(arg);
  }
}

ApplicationTestBase::ApplicationTestBase() : application_impl_(nullptr) {
}

ApplicationTestBase::~ApplicationTestBase() {
}

ApplicationDelegate* ApplicationTestBase::GetApplicationDelegate() {
  return &default_application_delegate_;
}

void ApplicationTestBase::SetUpWithArgs(const Array<String>& args) {
  // A run loop is needed for ApplicationImpl initialization and communication.
  Environment::InstantiateDefaultRunLoop();

  // New applications are constructed for each test to avoid persisting state.
  application_impl_ = new ApplicationImpl(GetApplicationDelegate(),
                                          PassShellHandle());

  // Fake application initialization with the given command line arguments.
  application_impl_->Initialize(args.Clone());
}

void ApplicationTestBase::SetUp() {
  SetUpWithArgs(Args());
}

void ApplicationTestBase::TearDown() {
  SetShellHandle(application_impl_->UnbindShell());
  delete application_impl_;
  Environment::DestroyDefaultRunLoop();
}

}  // namespace test
}  // namespace mojo
