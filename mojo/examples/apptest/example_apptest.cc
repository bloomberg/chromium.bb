// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include "mojo/examples/apptest/example_client_application.h"
#include "mojo/examples/apptest/example_client_impl.h"
#include "mojo/examples/apptest/example_service.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

// This global shell handle is needed for repeated use by test applications.
MessagePipeHandle g_shell_message_pipe_handle_hack;

// Apptest is a GTEST base class for application testing executed in mojo_shell.
class Apptest : public testing::Test {
 public:
  explicit Apptest(Array<String> args)
      : args_(args.Pass()),
        application_impl_(nullptr) {
  }
  virtual ~Apptest() override {}

 protected:
  ApplicationImpl* application_impl() { return application_impl_; }

  // Get the ApplicationDelegate for the application to be tested.
  virtual ApplicationDelegate* GetApplicationDelegate() = 0;

  // testing::Test:
  virtual void SetUp() override {
    // New applications are constructed for each test to avoid persisting state.
    MOJO_CHECK(g_shell_message_pipe_handle_hack.is_valid());
    application_impl_ = new ApplicationImpl(
        GetApplicationDelegate(),
        MakeScopedHandle(g_shell_message_pipe_handle_hack));

    // Fake application initialization with the given command line arguments.
    application_impl_->Initialize(args_.Clone());
  }
  virtual void TearDown() override {
    g_shell_message_pipe_handle_hack =
        application_impl_->UnbindShell().release();
    delete application_impl_;
  }

 private:
  // The command line arguments supplied to each test application instance.
  Array<String> args_;

  // The application implementation instance, reconstructed for each test.
  ApplicationImpl* application_impl_;

  // A run loop is needed for ApplicationImpl initialization and communication.
  RunLoop run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Apptest);
};

// ExampleApptest exemplifies Apptest's application testing pattern.
class ExampleApptest : public Apptest {
 public:
  // TODO(msw): Exemplify the use of actual command line arguments.
  ExampleApptest() : Apptest(Array<String>()) {}
  virtual ~ExampleApptest() override {}

 protected:
  // Apptest:
  virtual ApplicationDelegate* GetApplicationDelegate() override {
    return &example_client_application_;
  }
  virtual void SetUp() override {
    Apptest::SetUp();
    application_impl()->ConnectToService("mojo:mojo_example_service",
                                         &example_service_);
    example_service_.set_client(&example_client_);
  }

  ExampleServicePtr example_service_;
  ExampleClientImpl example_client_;

 private:
  ExampleClientApplication example_client_application_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ExampleApptest);
};

TEST_F(ExampleApptest, PongClientDirectly) {
  // Test very basic standalone ExampleClient functionality.
  ExampleClientImpl example_client;
  EXPECT_EQ(0, example_client.last_pong_value());
  example_client.Pong(1);
  EXPECT_EQ(1, example_client.last_pong_value());
}

TEST_F(ExampleApptest, PingServiceToPongClient) {
  // Test ExampleClient and ExampleService interaction.
  EXPECT_EQ(0, example_client_.last_pong_value());
  example_service_->Ping(1);
  EXPECT_TRUE(example_service_.WaitForIncomingMethodCall());
  EXPECT_EQ(1, example_client_.last_pong_value());
}

template <typename T>
struct SetCallback : public Callback<void()>::Runnable {
  SetCallback(T* val, T result) : val_(val), result_(result) {}
  virtual ~SetCallback() {}
  virtual void Run() const override { *val_ = result_; }
  T* val_;
  T result_;
};

TEST_F(ExampleApptest, RunCallbackViaService) {
  // Test ExampleService callback functionality.
  bool was_run = false;
  example_service_->RunCallback(SetCallback<bool>(&was_run, true));
  EXPECT_TRUE(example_service_.WaitForIncomingMethodCall());
  EXPECT_TRUE(was_run);
}

}  // namespace

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::Environment environment;

  {
    // This RunLoop is used for init, and then destroyed before running tests.
    mojo::RunLoop run_loop;

    // Construct an ApplicationImpl just for the GTEST commandline arguments.
    // GTEST command line arguments are supported amid application arguments:
    //   mojo_shell 'mojo:mojo_example_apptest arg1 --gtest_filter=foo arg2'
    mojo::ApplicationDelegate dummy_application_delegate;
    mojo::ApplicationImpl app(&dummy_application_delegate, shell_handle);
    MOJO_CHECK(app.WaitForInitialize());

    // InitGoogleTest expects (argc + 1) elements, including a terminating NULL.
    // It also removes GTEST arguments from |argv| and updates the |argc| count.
    // TODO(msw): Provide tests access to these actual command line arguments.
    const std::vector<std::string>& args = app.args();
    MOJO_CHECK(args.size() < INT_MAX);
    int argc = static_cast<int>(args.size());
    std::vector<char*> argv(argc + 1);
    for (int i = 0; i < argc; ++i)
      argv[i] = const_cast<char*>(args[i].data());
    argv[argc] = NULL;
    testing::InitGoogleTest(&argc, &argv[0]);
    mojo::g_shell_message_pipe_handle_hack = app.UnbindShell().release();
  }

  mojo_ignore_result(RUN_ALL_TESTS());

  MojoResult result = MojoClose(mojo::g_shell_message_pipe_handle_hack.value());
  MOJO_ALLOW_UNUSED_LOCAL(result);
  assert(result == MOJO_RESULT_OK);

  return MOJO_RESULT_OK;
}
