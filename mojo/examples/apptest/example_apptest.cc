// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/apptest/example_client_impl.h"
#include "mojo/examples/apptest/example_service.mojom.h"
#include "mojo/public/cpp/application/application_delegate.h"
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

class ExampleServiceTest : public testing::Test {
 public:
  ExampleServiceTest() {
    g_application_impl_hack->ConnectToService("mojo:mojo_example_service",
                                              &example_service_);
    example_service_.set_client(&example_client_);
  }

  virtual ~ExampleServiceTest() MOJO_OVERRIDE {}

 protected:
  ExampleServicePtr example_service_;
  ExampleClientImpl example_client_;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ExampleServiceTest);
};

TEST_F(ExampleServiceTest, Ping) {
  EXPECT_EQ(0, example_client_.last_pong_value());
  example_service_->Ping(1);
  mojo::RunLoop::current()->Run();
  EXPECT_EQ(1, example_client_.last_pong_value());
}

template <typename T>
struct SetAndQuit : public Callback<void()>::Runnable {
  SetAndQuit(T* val, T result) : val_(val), result_(result) {}
  virtual ~SetAndQuit() {}
  virtual void Run() const MOJO_OVERRIDE{
    *val_ = result_;
    mojo::RunLoop::current()->Quit();
  }
  T* val_;
  T result_;
};

TEST_F(ExampleServiceTest, RunCallback) {
  bool was_run = false;
  example_service_->RunCallback(SetAndQuit<bool>(&was_run, true));
  mojo::RunLoop::current()->Run();
  EXPECT_TRUE(was_run);
}

}  // namespace

}  // namespace mojo

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;

  mojo::ApplicationDelegate* delegate = mojo::ApplicationDelegate::Create();
  mojo::ApplicationImpl app(delegate);
  app.BindShell(shell_handle);
  g_application_impl_hack = &app;

  // TODO(msw): Get actual commandline arguments.
  int argc = 0;
  char** argv = NULL;
  testing::InitGoogleTest(&argc, argv);
  mojo_ignore_result(RUN_ALL_TESTS());

  delete delegate;
  return MOJO_RESULT_OK;
}
