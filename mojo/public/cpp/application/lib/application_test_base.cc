// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_test_base.h"

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace test {

namespace {

// This shell handle is shared by multiple test application instances.
MessagePipeHandle g_shell_handle;

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

ApplicationTestBase::ApplicationTestBase(Array<String> args)
    : args_(args.Pass()), application_impl_(nullptr) {
}

ApplicationTestBase::~ApplicationTestBase() {
}

void ApplicationTestBase::SetUp() {
  // A run loop is needed for ApplicationImpl initialization and communication.
  Environment::InstantiateDefaultRunLoop();

  // New applications are constructed for each test to avoid persisting state.
  application_impl_ = new ApplicationImpl(GetApplicationDelegate(),
                                          PassShellHandle());

  // Fake application initialization with the given command line arguments.
  application_impl_->Initialize(args_.Clone());
}

void ApplicationTestBase::TearDown() {
  SetShellHandle(application_impl_->UnbindShell());
  delete application_impl_;
  Environment::DestroyDefaultRunLoop();
}

}  // namespace test
}  // namespace mojo
