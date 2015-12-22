// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/async_handle_waiter.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {
namespace internal {
namespace {

void ReadOneByteOfX(MojoHandle pipe) {
  uint32_t size = 1;
  char buffer = ' ';
  MojoResult rv = MojoReadMessage(pipe, &buffer, &size, nullptr, nullptr,
                                  MOJO_READ_MESSAGE_FLAG_NONE);
  CHECK_EQ(rv, MOJO_RESULT_OK);
  CHECK_EQ(size, 1U);
  CHECK_EQ(buffer, 'X');
}

class AsyncHandleWaiterTest : public testing::Test {
 public:
  AsyncHandleWaiterTest() : worker_("test_worker") {
    worker_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  }

  void SetUp() override {
    message_loop_.reset(new base::MessageLoopForIO());
    ResetSignaledStates();
    mojo::CreateMessagePipe(nullptr, &pipe_to_write_, &pipe_to_read_);
    target_.reset(new AsyncHandleWaiter(base::Bind(
        &AsyncHandleWaiterTest::HandleIsReady, base::Unretained(this))));
  }

 protected:
  MojoResult Start() {
    return target_->Wait(pipe_to_read_.get().value(),
                         MOJO_HANDLE_SIGNAL_READABLE);
  }

  void ResetSignaledStates() {
    signaled_result_ = MOJO_RESULT_UNKNOWN;
    run_loop_.reset(new base::RunLoop());
  }

  void WriteToPipe() {
    MojoResult rv = MojoWriteMessage(pipe_to_write_.get().value(), "X", 1,
                                     nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
    CHECK_EQ(rv, MOJO_RESULT_OK);
  }

  void WriteToPipeFromWorker() {
    worker_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&AsyncHandleWaiterTest::WriteToPipe,
                              base::Unretained(this)));
  }

  void WaitAndAssertSignaledAndMessageIsArrived() {
    run_loop_->Run();
    EXPECT_EQ(MOJO_RESULT_OK, signaled_result_);

    ReadOneByteOfX(pipe_to_read_.get().value());
  }

  void WaitAndAssertNotSignaled() {
    run_loop_->RunUntilIdle();
    EXPECT_EQ(MOJO_RESULT_OK, MojoWait(pipe_to_read_.get().value(),
                                       MOJO_HANDLE_SIGNAL_READABLE, 0,
                                       nullptr));
    EXPECT_EQ(MOJO_RESULT_UNKNOWN, signaled_result_);
  }

  void HandleIsReady(MojoResult result) {
    CHECK_EQ(base::MessageLoop::current(), message_loop_.get());
    CHECK_EQ(signaled_result_, MOJO_RESULT_UNKNOWN);
    signaled_result_ = result;
    run_loop_->Quit();
  }

  base::Thread worker_;
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  mojo::ScopedMessagePipeHandle pipe_to_write_;
  mojo::ScopedMessagePipeHandle pipe_to_read_;

  scoped_ptr<AsyncHandleWaiter> target_;
  MojoResult signaled_result_;
};

TEST_F(AsyncHandleWaiterTest, SignalFromSameThread) {
  EXPECT_EQ(MOJO_RESULT_OK, Start());
  WriteToPipe();
  WaitAndAssertSignaledAndMessageIsArrived();

  // Ensures that the waiter is reusable.
  ResetSignaledStates();

  EXPECT_EQ(MOJO_RESULT_OK, Start());
  WriteToPipe();
  WaitAndAssertSignaledAndMessageIsArrived();
}

TEST_F(AsyncHandleWaiterTest, SignalFromDifferentThread) {
  EXPECT_EQ(MOJO_RESULT_OK, Start());
  WriteToPipeFromWorker();
  WaitAndAssertSignaledAndMessageIsArrived();

  // Ensures that the waiter is reusable.
  ResetSignaledStates();

  EXPECT_EQ(MOJO_RESULT_OK, Start());
  WriteToPipeFromWorker();
  WaitAndAssertSignaledAndMessageIsArrived();
}

TEST_F(AsyncHandleWaiterTest, DeleteWaiterBeforeWrite) {
  EXPECT_EQ(MOJO_RESULT_OK, Start());

  target_.reset();

  WriteToPipe();
  WaitAndAssertNotSignaled();
}

TEST_F(AsyncHandleWaiterTest, DeleteWaiterBeforeSignal) {
  EXPECT_EQ(MOJO_RESULT_OK, Start());
  WriteToPipe();

  target_.reset();

  WaitAndAssertNotSignaled();
}

class HandlerThatReenters {
 public:
  HandlerThatReenters(base::RunLoop* loop, MojoHandle handle)
      : target_(nullptr), handle_(handle), loop_(loop), step_(0) {}

  void set_target(AsyncHandleWaiter* target) { target_ = target; }

  void HandleIsReady(MojoResult result) {
    switch (step_) {
      case 0:
        RestartAndClose(result);
        break;
      case 1:
        HandleClosingSignal(result);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void RestartAndClose(MojoResult result) {
    CHECK_EQ(step_, 0);
    CHECK_EQ(result, MOJO_RESULT_OK);
    step_ = 1;

    ReadOneByteOfX(handle_);
    target_->Wait(handle_, MOJO_HANDLE_SIGNAL_READABLE);

    // This signals the |AsyncHandleWaiter|.
    MojoResult rv = MojoClose(handle_);
    CHECK_EQ(rv, MOJO_RESULT_OK);
  }

  void HandleClosingSignal(MojoResult result) {
    CHECK_EQ(step_, 1);
    CHECK_EQ(result, MOJO_RESULT_CANCELLED);
    step_ = 2;
    loop_->Quit();
  }

  bool IsClosingHandled() const { return step_ == 2; }

  AsyncHandleWaiter* target_;
  MojoHandle handle_;
  base::RunLoop* loop_;
  int step_;
};

TEST_F(AsyncHandleWaiterTest, RestartWaitingWhileSignaled) {
  HandlerThatReenters handler(run_loop_.get(), pipe_to_read_.get().value());
  target_.reset(new AsyncHandleWaiter(base::Bind(
      &HandlerThatReenters::HandleIsReady, base::Unretained(&handler))));
  handler.set_target(target_.get());

  EXPECT_EQ(MOJO_RESULT_OK, Start());
  WriteToPipe();
  run_loop_->Run();

  EXPECT_TRUE(handler.IsClosingHandled());

  // |HandlerThatReenters::RestartAndClose| already closed it.
  ::ignore_result(pipe_to_read_.release());
}

class AsyncHandleWaiterIOObserverTest : public testing::Test {
 public:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoopForIO());
    target_.reset(new AsyncHandleWaiter(
        base::Bind(&AsyncHandleWaiterIOObserverTest::HandleIsReady,
                   base::Unretained(this))));
    invocation_count_ = 0;
  }

  void HandleIsReady(MojoResult result) { invocation_count_++; }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<AsyncHandleWaiter> target_;
  size_t invocation_count_;
};

TEST_F(AsyncHandleWaiterIOObserverTest, OutsideIOEvnet) {
  target_->GetWaitCallbackForTest().Run(MOJO_RESULT_OK);
  EXPECT_EQ(0U, invocation_count_);
  message_loop_->RunUntilIdle();
  EXPECT_EQ(1U, invocation_count_);
}

TEST_F(AsyncHandleWaiterIOObserverTest, InsideIOEvnet) {
  target_->GetIOObserverForTest()->WillProcessIOEvent();
  target_->GetWaitCallbackForTest().Run(MOJO_RESULT_OK);
  EXPECT_EQ(0U, invocation_count_);
  target_->GetIOObserverForTest()->DidProcessIOEvent();
  EXPECT_EQ(1U, invocation_count_);
}

TEST_F(AsyncHandleWaiterIOObserverTest, Reenter) {
  target_->GetIOObserverForTest()->WillProcessIOEvent();
  target_->GetWaitCallbackForTest().Run(MOJO_RESULT_OK);
  EXPECT_EQ(0U, invocation_count_);

  // As if some other io handler start nested loop.
  target_->GetIOObserverForTest()->WillProcessIOEvent();
  target_->GetWaitCallbackForTest().Run(MOJO_RESULT_OK);
  target_->GetIOObserverForTest()->DidProcessIOEvent();
  EXPECT_EQ(0U, invocation_count_);

  target_->GetIOObserverForTest()->DidProcessIOEvent();
  EXPECT_EQ(1U, invocation_count_);
}

}  // namespace
}  // namespace internal
}  // namespace IPC
