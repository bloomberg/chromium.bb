// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/apptest/example_client_application.h"
#include "mojo/examples/apptest/example_client_impl.h"
#include "mojo/examples/apptest/example_service.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(msw): Remove this once we can get ApplicationImpl from TLS.
mojo::ApplicationImpl* g_application_impl_hack = NULL;

}  // namespace

namespace mojo {

namespace {

class ExampleApptest : public testing::Test {
 public:
  ExampleApptest() {
    g_application_impl_hack->ConnectToService("mojo:mojo_example_service",
                                              &example_service_);
    example_service_.set_client(&example_client_);
  }

  virtual ~ExampleApptest() override {}

 protected:
  ExampleServicePtr example_service_;
  ExampleClientImpl example_client_;

 private:
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
struct SetAndQuit : public Callback<void()>::Runnable {
  SetAndQuit(T* val, T result) : val_(val), result_(result) {}
  virtual ~SetAndQuit() {}
  virtual void Run() const override { *val_ = result_; }
  T* val_;
  T result_;
};

TEST_F(ExampleApptest, RunCallbackViaService) {
  // Test ExampleService callback functionality.
  bool was_run = false;
  example_service_->RunCallback(SetAndQuit<bool>(&was_run, true));
  EXPECT_TRUE(example_service_.WaitForIncomingMethodCall());
  EXPECT_TRUE(was_run);
}

}  // namespace

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::Environment env;
  // TODO(msw): Destroy this ambient RunLoop before running tests.
  //            Need to CancelWait() / PassMessagePipe() from the ShellPtr?
  mojo::RunLoop loop;
  mojo::ApplicationDelegate* delegate = new mojo::ExampleClientApplication();
  mojo::ApplicationImpl app(delegate, shell_handle);
  g_application_impl_hack = &app;
  MOJO_CHECK(app.WaitForInitialize());

  // TODO(msw): Plumb commandline arguments through app->args().
  int argc = 0;
  char** argv = NULL;
  testing::InitGoogleTest(&argc, argv);
  mojo_ignore_result(RUN_ALL_TESTS());

  delete delegate;
  return MOJO_RESULT_OK;
}
