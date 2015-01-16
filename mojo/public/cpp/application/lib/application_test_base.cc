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

void InitializeArgs(int argc, std::vector<const char*> argv) {
  MOJO_CHECK(g_args.is_null());
  for (const char* arg : argv) {
    if (arg)
      g_args.push_back(arg);
  }
}

}  // namespace

const Array<String>& Args() {
  return g_args;
}

MojoResult RunAllTests(MojoHandle shell_handle) {
  {
    // This loop is used for init, and then destroyed before running tests.
    Environment::InstantiateDefaultRunLoop();

    // Construct an ApplicationImpl just for the GTEST commandline arguments.
    // GTEST command line arguments are supported amid application arguments:
    // $ mojo_shell mojo:example_apptests
    //   --args-for='mojo:example_apptests arg1 --gtest_filter=foo arg2'
    mojo::ApplicationDelegate dummy_application_delegate;
    mojo::ApplicationImpl app(&dummy_application_delegate, shell_handle);
    MOJO_CHECK(app.WaitForInitialize());

    // InitGoogleTest expects (argc + 1) elements, including a terminating null.
    // It also removes GTEST arguments from |argv| and updates the |argc| count.
    const std::vector<std::string>& args = app.args();
    MOJO_CHECK(args.size() <
               static_cast<size_t>(std::numeric_limits<int>::max()));
    int argc = static_cast<int>(args.size());
    std::vector<const char*> argv(argc + 1);
    for (int i = 0; i < argc; ++i)
      argv[i] = args[i].c_str();
    argv[argc] = nullptr;

    testing::InitGoogleTest(&argc, const_cast<char**>(&(argv[0])));
    SetShellHandle(app.UnbindShell());
    InitializeArgs(argc, argv);

    Environment::DestroyDefaultRunLoop();
  }

  int result = RUN_ALL_TESTS();

  shell_handle = mojo::test::PassShellHandle().release().value();
  MojoResult close_result = MojoClose(shell_handle);
  MOJO_CHECK(close_result == MOJO_RESULT_OK);

  return (result == 0) ? MOJO_RESULT_OK : MOJO_RESULT_UNKNOWN;
}

ApplicationTestBase::ApplicationTestBase() : application_impl_(nullptr) {
}

ApplicationTestBase::~ApplicationTestBase() {
}

ApplicationDelegate* ApplicationTestBase::GetApplicationDelegate() {
  return &default_application_delegate_;
}

void ApplicationTestBase::SetUpWithArgs(const Array<String>& args) {
  // A run loop is recommended for ApplicationImpl initialization and
  // communication.
  if (ShouldCreateDefaultRunLoop())
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
  if (ShouldCreateDefaultRunLoop())
    Environment::DestroyDefaultRunLoop();
}

bool ApplicationTestBase::ShouldCreateDefaultRunLoop() {
  return true;
}

}  // namespace test
}  // namespace mojo
