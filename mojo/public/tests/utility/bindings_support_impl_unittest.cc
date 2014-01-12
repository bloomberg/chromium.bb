// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/utility/bindings_support_impl.h"

#include "base/basictypes.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/public/tests/test_support.h"
#include "mojo/public/utility/environment.h"
#include "mojo/public/utility/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class TestAsyncWaitCallback : public BindingsSupport::AsyncWaitCallback {
 public:
  TestAsyncWaitCallback() : result_count_(0), last_result_(MOJO_RESULT_OK) {
  }
  virtual ~TestAsyncWaitCallback() {}

  int result_count() const { return result_count_; }

  MojoResult last_result() const { return last_result_; }

  // RunLoopHandler:
  virtual void OnHandleReady(MojoResult result) OVERRIDE {
    result_count_++;
    last_result_ = result;
  }

 private:
  int result_count_;
  MojoResult last_result_;

  DISALLOW_COPY_AND_ASSIGN(TestAsyncWaitCallback);
};

class BindingsSupportImplTest : public testing::Test {
 public:
  BindingsSupportImplTest() {}

  virtual void SetUp() OVERRIDE {
    Test::SetUp();
    environment_.reset(new Environment);
    run_loop_.reset(new RunLoop);
  }
  virtual void TearDown() OVERRIDE {
    run_loop_.reset();
    environment_.reset(NULL);
    Test::TearDown();
  }

 private:
  scoped_ptr<Environment> environment_;
  scoped_ptr<RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BindingsSupportImplTest);
};

// Verifies AsyncWaitCallback is notified when pipe is ready.
TEST_F(BindingsSupportImplTest, CallbackNotified) {
  TestAsyncWaitCallback callback;
  MessagePipe test_pipe;
  EXPECT_EQ(MOJO_RESULT_OK,
            test::WriteEmptyMessage(test_pipe.handle1.get()));

  BindingsSupport::Get()->AsyncWait(test_pipe.handle0.get(),
                                    MOJO_WAIT_FLAG_READABLE, &callback);
  RunLoop::current()->Run();
  EXPECT_EQ(1, callback.result_count());
  EXPECT_EQ(MOJO_RESULT_OK, callback.last_result());
}

// Verifies 2 AsyncWaitCallbacks are notified when there pipes are ready.
TEST_F(BindingsSupportImplTest, TwoCallbacksNotified) {
  TestAsyncWaitCallback callback1;
  TestAsyncWaitCallback callback2;
  MessagePipe test_pipe1;
  MessagePipe test_pipe2;
  EXPECT_EQ(MOJO_RESULT_OK,
            test::WriteEmptyMessage(test_pipe1.handle1.get()));
  EXPECT_EQ(MOJO_RESULT_OK,
            test::WriteEmptyMessage(test_pipe2.handle1.get()));

  BindingsSupport::Get()->AsyncWait(test_pipe1.handle0.get(),
                                    MOJO_WAIT_FLAG_READABLE, &callback1);
  BindingsSupport::Get()->AsyncWait(test_pipe2.handle0.get(),
                                    MOJO_WAIT_FLAG_READABLE, &callback2);
  RunLoop::current()->Run();
  EXPECT_EQ(1, callback1.result_count());
  EXPECT_EQ(MOJO_RESULT_OK, callback1.last_result());
  EXPECT_EQ(1, callback2.result_count());
  EXPECT_EQ(MOJO_RESULT_OK, callback2.last_result());
}

// Verifies cancel works.
TEST_F(BindingsSupportImplTest, CancelCallback) {
  TestAsyncWaitCallback callback;
  MessagePipe test_pipe;
  EXPECT_EQ(MOJO_RESULT_OK, test::WriteEmptyMessage(test_pipe.handle1.get()));

  BindingsSupport::Get()->CancelWait(
      BindingsSupport::Get()->AsyncWait(test_pipe.handle0.get(),
                                        MOJO_WAIT_FLAG_READABLE, &callback));
  RunLoop::current()->Run();
  EXPECT_EQ(0, callback.result_count());
}

}  // namespace
}  // namespace mojo
