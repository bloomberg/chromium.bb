// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/utility/run_loop.h"

#include <string>

#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/cpp/utility/run_loop_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class TestRunLoopHandler : public RunLoopHandler {
 public:
  TestRunLoopHandler()
      : ready_count_(0),
        error_count_(0),
        last_error_result_(MOJO_RESULT_OK) {
  }
  virtual ~TestRunLoopHandler() {}

  void clear_ready_count() { ready_count_ = 0; }
  int ready_count() const { return ready_count_; }

  void clear_error_count() { error_count_ = 0; }
  int error_count() const { return error_count_; }

  MojoResult last_error_result() const { return last_error_result_; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    ready_count_++;
  }
  virtual void OnHandleError(const Handle& handle, MojoResult result)
      MOJO_OVERRIDE {
    error_count_++;
    last_error_result_ = result;
  }

 private:
  int ready_count_;
  int error_count_;
  MojoResult last_error_result_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestRunLoopHandler);
};

class RunLoopTest : public testing::Test {
 public:
  RunLoopTest() {}

  virtual void SetUp() MOJO_OVERRIDE {
    Test::SetUp();
    RunLoop::SetUp();
  }
  virtual void TearDown() MOJO_OVERRIDE {
    RunLoop::TearDown();
    Test::TearDown();
  }

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(RunLoopTest);
};

// Trivial test to verify Run() with no added handles returns.
TEST_F(RunLoopTest, ExitsWithNoHandles) {
  RunLoop run_loop;
  run_loop.Run();
}

class RemoveOnReadyRunLoopHandler : public TestRunLoopHandler {
 public:
  RemoveOnReadyRunLoopHandler() : run_loop_(NULL) {
  }
  virtual ~RemoveOnReadyRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    run_loop_->RemoveHandler(handle);
    TestRunLoopHandler::OnHandleReady(handle);
  }

 private:
  RunLoop* run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RemoveOnReadyRunLoopHandler);
};

// Verifies RunLoop quits when no more handles (handle is removed when ready).
TEST_F(RunLoopTest, HandleReady) {
  RemoveOnReadyRunLoopHandler handler;
  MessagePipe test_pipe;
  EXPECT_TRUE(test::WriteTextMessage(test_pipe.handle1.get(), std::string()));

  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE);
  run_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_FALSE(run_loop.HasHandler(test_pipe.handle0.get()));
}

class QuitOnReadyRunLoopHandler : public TestRunLoopHandler {
 public:
  QuitOnReadyRunLoopHandler() : run_loop_(NULL) {
  }
  virtual ~QuitOnReadyRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    run_loop_->Quit();
    TestRunLoopHandler::OnHandleReady(handle);
  }

 private:
  RunLoop* run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(QuitOnReadyRunLoopHandler);
};

// Verifies Quit() from OnHandleReady() quits the loop.
TEST_F(RunLoopTest, QuitFromReady) {
  QuitOnReadyRunLoopHandler handler;
  MessagePipe test_pipe;
  EXPECT_TRUE(test::WriteTextMessage(test_pipe.handle1.get(), std::string()));

  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE);
  run_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_TRUE(run_loop.HasHandler(test_pipe.handle0.get()));
}

class QuitOnErrorRunLoopHandler : public TestRunLoopHandler {
 public:
  QuitOnErrorRunLoopHandler() : run_loop_(NULL) {
  }
  virtual ~QuitOnErrorRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }

  // RunLoopHandler:
  virtual void OnHandleError(const Handle& handle, MojoResult result)
      MOJO_OVERRIDE {
    run_loop_->Quit();
    TestRunLoopHandler::OnHandleError(handle, result);
  }

 private:
  RunLoop* run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(QuitOnErrorRunLoopHandler);
};

// Verifies Quit() when the deadline is reached works.
TEST_F(RunLoopTest, QuitWhenDeadlineExpired) {
  QuitOnErrorRunLoopHandler handler;
  MessagePipe test_pipe;
  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE,
                      static_cast<MojoDeadline>(10000));
  run_loop.Run();
  EXPECT_EQ(0, handler.ready_count());
  EXPECT_EQ(1, handler.error_count());
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, handler.last_error_result());
  EXPECT_FALSE(run_loop.HasHandler(test_pipe.handle0.get()));
}

TEST_F(RunLoopTest, Current) {
  EXPECT_TRUE(RunLoop::current() == NULL);
  {
    RunLoop run_loop;
    EXPECT_EQ(&run_loop, RunLoop::current());
  }
  EXPECT_TRUE(RunLoop::current() == NULL);
}

class NestingRunLoopHandler : public TestRunLoopHandler {
 public:
  static const size_t kDepthLimit;
  static const char kSignalMagic;

  NestingRunLoopHandler()
      : run_loop_(NULL),
        pipe_(NULL),
        depth_(0),
        reached_depth_limit_(false) {}

  virtual ~NestingRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }
  void set_pipe(MessagePipe* pipe) { pipe_ = pipe; }
  bool reached_depth_limit() const { return reached_depth_limit_; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    TestRunLoopHandler::OnHandleReady(handle);
    EXPECT_EQ(handle.value(), pipe_->handle0.get().value());

    ReadSignal();
    size_t current_depth = ++depth_;
    if (current_depth < kDepthLimit) {
      WriteSignal();
      run_loop_->Run();
      if (current_depth == kDepthLimit - 1) {
        // The topmost loop Quit()-ed, so its parent takes back the
        // control without exeeding deadline.
        EXPECT_EQ(error_count(), 0);
      } else {
        EXPECT_EQ(error_count(), 1);
      }

    } else {
      EXPECT_EQ(current_depth, kDepthLimit);
      reached_depth_limit_ = true;
      run_loop_->Quit();
    }
    --depth_;
  }

  void WriteSignal() {
    char write_byte = kSignalMagic;
    MojoResult write_result = WriteMessageRaw(
        pipe_->handle1.get(),
        &write_byte, 1, NULL, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
    EXPECT_EQ(write_result, MOJO_RESULT_OK);
  }

  void ReadSignal() {
    char read_byte = 0;
    uint32_t bytes_read = 1;
    uint32_t handles_read = 0;
    MojoResult read_result = ReadMessageRaw(
        pipe_->handle0.get(),
        &read_byte, &bytes_read, NULL, &handles_read,
        MOJO_READ_MESSAGE_FLAG_NONE);
    EXPECT_EQ(read_result, MOJO_RESULT_OK);
    EXPECT_EQ(read_byte, kSignalMagic);
  }

 private:
  RunLoop* run_loop_;
  MessagePipe* pipe_;
  size_t depth_;
  bool reached_depth_limit_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(NestingRunLoopHandler);
};

const size_t NestingRunLoopHandler::kDepthLimit = 10;
const char NestingRunLoopHandler::kSignalMagic = 'X';

TEST_F(RunLoopTest, NestedRun) {
  NestingRunLoopHandler handler;
  MessagePipe test_pipe;
  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  handler.set_pipe(&test_pipe);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE,
                      static_cast<MojoDeadline>(10000));
  handler.WriteSignal();
  run_loop.Run();

  EXPECT_TRUE(handler.reached_depth_limit());
  // Got MOJO_RESULT_DEADLINE_EXCEEDED once then removed from the
  // RunLoop's handler list.
  EXPECT_EQ(handler.error_count(), 1);
  EXPECT_EQ(handler.last_error_result(), MOJO_RESULT_DEADLINE_EXCEEDED);
}

}  // namespace
}  // namespace mojo
