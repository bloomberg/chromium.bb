// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_test_base.h"

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/utility/lib/thread_local.h"
#include "mojo/public/cpp/utility/run_loop.h"

namespace mojo {
namespace test {

namespace {

// This global shell handle is needed for repeated use by test applications.
MessagePipeHandle test_shell_handle;

// TODO(msw): Support base::MessageLoop environments.
internal::ThreadLocalPointer<RunLoop> test_run_loop;

}  // namespace

ApplicationTestBase::ApplicationTestBase(Array<String> args)
    : args_(args.Pass()), application_impl_(nullptr) {
}

ApplicationTestBase::~ApplicationTestBase() {
}

void ApplicationTestBase::SetUp() {
  // A run loop is needed for ApplicationImpl initialization and communication.
  test_run_loop.Set(new RunLoop());

  // New applications are constructed for each test to avoid persisting state.
  MOJO_CHECK(test_shell_handle.is_valid());
  application_impl_ = new ApplicationImpl(GetApplicationDelegate(),
                                          MakeScopedHandle(test_shell_handle));

  // Fake application initialization with the given command line arguments.
  application_impl_->Initialize(args_.Clone());
}

void ApplicationTestBase::TearDown() {
  test_shell_handle = application_impl_->UnbindShell().release();
  delete application_impl_;
  delete test_run_loop.Get();
  test_run_loop.Set(nullptr);
}

}  // namespace test
}  // namespace mojo

// TODO(msw): Split this into an application_test_main.cc.
MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::Environment environment;

  {
    // This RunLoop is used for init, and then destroyed before running tests.
    mojo::RunLoop run_loop;

    // Construct an ApplicationImpl just for the GTEST commandline arguments.
    // GTEST command line arguments are supported amid application arguments:
    // $ mojo_shell 'mojo:example_apptest arg1 --gtest_filter=foo arg2'
    mojo::ApplicationDelegate dummy_application_delegate;
    mojo::ApplicationImpl app(&dummy_application_delegate, shell_handle);
    MOJO_CHECK(app.WaitForInitialize());

    // InitGoogleTest expects (argc + 1) elements, including a terminating NULL.
    // It also removes GTEST arguments from |argv| and updates the |argc| count.
    // TODO(msw): Provide tests access to these actual command line arguments.
    const std::vector<std::string>& args = app.args();
    MOJO_CHECK(args.size() <
               static_cast<size_t>(std::numeric_limits<int>::max()));
    int argc = static_cast<int>(args.size());
    std::vector<const char*> argv(argc + 1);
    for (int i = 0; i < argc; ++i)
      argv[i] = args[i].c_str();
    argv[argc] = nullptr;
    testing::InitGoogleTest(&argc, const_cast<char**>(&(argv[0])));
    mojo::test::test_shell_handle = app.UnbindShell().release();
  }

  int result = RUN_ALL_TESTS();

  MojoResult close_result = MojoClose(mojo::test::test_shell_handle.value());
  MOJO_CHECK(close_result == MOJO_RESULT_OK);

  return (result == 0) ? MOJO_RESULT_OK : MOJO_RESULT_UNKNOWN;
}
