// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/handle_watcher.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "mojo/common/scoped_message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {

MojoResult WriteToHandle(MojoHandle handle) {
  return MojoWriteMessage(handle, NULL, 0, NULL, 0,
                          MOJO_WRITE_MESSAGE_FLAG_NONE);
}

MojoResult ReadFromHandle(MojoHandle handle) {
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  return MojoReadMessage(handle, NULL, &num_bytes, NULL, &num_handles,
                         MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
}

void RunUntilIdle() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void DeleteWatcherAndForwardResult(
    HandleWatcher* watcher,
    base::Callback<void(MojoResult)> next_callback,
    MojoResult result) {
  delete watcher;
  next_callback.Run(result);
}

// Helper class to manage the callback and running the message loop waiting for
// message to be received. Typical usage is something like:
//   Schedule callback returned from GetCallback().
//   RunUntilGotCallback();
//   EXPECT_TRUE(got_callback());
//   clear_callback();
class CallbackHelper {
 public:
  CallbackHelper()
      : got_callback_(false),
        run_loop_(NULL),
        weak_factory_(this) {}
  ~CallbackHelper() {}

  // See description above |got_callback_|.
  bool got_callback() const { return got_callback_; }
  void clear_callback() { got_callback_ = false; }

  // Runs the current MessageLoop until the callback returned from GetCallback()
  // is notified.
  void RunUntilGotCallback() {
    ASSERT_TRUE(run_loop_ == NULL);
    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> reseter(&run_loop_, &run_loop);
    run_loop.Run();
  }

  base::Callback<void(MojoResult)> GetCallback() {
    return base::Bind(&CallbackHelper::OnCallback, weak_factory_.GetWeakPtr());
  }

  void Start(HandleWatcher* watcher, MojoHandle handle) {
    StartWithCallback(watcher, handle, GetCallback());
  }

  void StartWithCallback(HandleWatcher* watcher, MojoHandle handle,
                         const base::Callback<void(MojoResult)>& callback) {
    watcher->Start(handle, MOJO_WAIT_FLAG_READABLE, MOJO_DEADLINE_INDEFINITE,
                   callback);
  }

 private:
  void OnCallback(MojoResult result) {
    got_callback_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  // Set to true when the callback is called.
  bool got_callback_;

  // If non-NULL we're in RunUntilGotCallback().
  base::RunLoop* run_loop_;

  base::WeakPtrFactory<CallbackHelper> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

class HandleWatcherTest : public testing::Test {
 public:
  HandleWatcherTest() {}
  virtual ~HandleWatcherTest() {
    HandleWatcher::tick_clock_ = NULL;
  }

 protected:
  void InstallTickClock() {
    HandleWatcher::tick_clock_ = &tick_clock_;
  }

  base::SimpleTestTickClock tick_clock_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(HandleWatcherTest);
};

// Trivial test case with a single handle to watch.
TEST_F(HandleWatcherTest, SingleHandler) {
  ScopedMessagePipe test_pipe;
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe.handle_0());
  CallbackHelper callback_helper;
  HandleWatcher watcher;
  callback_helper.Start(&watcher, test_pipe.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper.got_callback());
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe.handle_1()));
  callback_helper.RunUntilGotCallback();
  EXPECT_TRUE(callback_helper.got_callback());
}

// Creates three handles and notfies them in reverse order ensuring each one is
// notified appropriately.
TEST_F(HandleWatcherTest, ThreeHandles) {
  ScopedMessagePipe test_pipe1;
  ScopedMessagePipe test_pipe2;
  ScopedMessagePipe test_pipe3;
  CallbackHelper callback_helper1;
  CallbackHelper callback_helper2;
  CallbackHelper callback_helper3;
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe1.handle_0());
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe2.handle_0());
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe3.handle_0());

  HandleWatcher watcher1;
  callback_helper1.Start(&watcher1, test_pipe1.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());

  HandleWatcher watcher2;
  callback_helper2.Start(&watcher2, test_pipe2.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());

  HandleWatcher watcher3;
  callback_helper3.Start(&watcher3, test_pipe3.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());

  // Write to 3 and make sure it's notified.
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe3.handle_1()));
  callback_helper3.RunUntilGotCallback();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_TRUE(callback_helper3.got_callback());
  callback_helper3.clear_callback();

  // Write to 1 and 3. Only 1 should be notified since 3 was is no longer
  // running.
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe1.handle_1()));
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe3.handle_1()));
  callback_helper1.RunUntilGotCallback();
  EXPECT_TRUE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());
  callback_helper1.clear_callback();

  // Write to 1 and 2. Only 2 should be notified (since 1 was already notified).
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe1.handle_1()));
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe2.handle_1()));
  callback_helper2.RunUntilGotCallback();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_TRUE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());
}

// Verifies Start() invoked a second time works.
TEST_F(HandleWatcherTest, Restart) {
  ScopedMessagePipe test_pipe1;
  ScopedMessagePipe test_pipe2;
  CallbackHelper callback_helper1;
  CallbackHelper callback_helper2;
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe1.handle_0());
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe2.handle_0());

  HandleWatcher watcher1;
  callback_helper1.Start(&watcher1, test_pipe1.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());

  HandleWatcher watcher2;
  callback_helper2.Start(&watcher2, test_pipe2.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());

  // Write to 1 and make sure it's notified.
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe1.handle_1()));
  callback_helper1.RunUntilGotCallback();
  EXPECT_TRUE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  callback_helper1.clear_callback();
  EXPECT_EQ(MOJO_RESULT_OK, ReadFromHandle(test_pipe1.handle_0()));

  // Write to 2 and make sure it's notified.
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe2.handle_1()));
  callback_helper2.RunUntilGotCallback();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_TRUE(callback_helper2.got_callback());
  callback_helper2.clear_callback();

  // Listen on 1 again.
  callback_helper1.Start(&watcher1, test_pipe1.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());

  // Write to 1 and make sure it's notified.
  EXPECT_EQ(MOJO_RESULT_OK, WriteToHandle(test_pipe1.handle_1()));
  callback_helper1.RunUntilGotCallback();
  EXPECT_TRUE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
}

// Verifies deadline is honored.
TEST_F(HandleWatcherTest, Deadline) {
  InstallTickClock();

  ScopedMessagePipe test_pipe1;
  ScopedMessagePipe test_pipe2;
  ScopedMessagePipe test_pipe3;
  CallbackHelper callback_helper1;
  CallbackHelper callback_helper2;
  CallbackHelper callback_helper3;
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe1.handle_0());
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe2.handle_0());
  ASSERT_NE(MOJO_HANDLE_INVALID, test_pipe3.handle_0());

  // Add a watcher with an infinite timeout.
  HandleWatcher watcher1;
  callback_helper1.Start(&watcher1, test_pipe1.handle_0());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());

  // Add another watcher wth a timeout of 500 microseconds.
  HandleWatcher watcher2;
  watcher2.Start(test_pipe2.handle_0(), MOJO_WAIT_FLAG_READABLE, 500,
                 callback_helper2.GetCallback());
  RunUntilIdle();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_FALSE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());

  // Advance the clock passed the deadline. We also have to start another
  // watcher to wake up the background thread.
  tick_clock_.Advance(base::TimeDelta::FromMicroseconds(501));

  HandleWatcher watcher3;
  callback_helper3.Start(&watcher3, test_pipe3.handle_0());

  callback_helper2.RunUntilGotCallback();
  EXPECT_FALSE(callback_helper1.got_callback());
  EXPECT_TRUE(callback_helper2.got_callback());
  EXPECT_FALSE(callback_helper3.got_callback());
}

TEST_F(HandleWatcherTest, DeleteInCallback) {
  ScopedMessagePipe test_pipe;
  CallbackHelper callback_helper;

  HandleWatcher* watcher = new HandleWatcher();
  callback_helper.StartWithCallback(watcher, test_pipe.handle_1(),
                                    base::Bind(&DeleteWatcherAndForwardResult,
                                               watcher,
                                               callback_helper.GetCallback()));
  WriteToHandle(test_pipe.handle_0());
  callback_helper.RunUntilGotCallback();
  EXPECT_TRUE(callback_helper.got_callback());
}

}  // namespace test
}  // namespace common
}  // namespace mojo
