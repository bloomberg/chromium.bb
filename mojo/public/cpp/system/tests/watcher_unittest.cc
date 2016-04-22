// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/watcher.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

template <typename Handler>
void RunResultHandler(Handler f, MojoResult result) { f(result); }

template <typename Handler>
Watcher::ReadyCallback OnReady(Handler f) {
  return base::Bind(&RunResultHandler<Handler>, f);
}

Watcher::ReadyCallback NotReached() {
  return OnReady([] (MojoResult) { NOTREACHED(); });
}

class WatcherTest : public testing::Test {
 public:
  WatcherTest() {}
  ~WatcherTest() override {}

  void SetUp() override {
    message_loop_.reset(new base::MessageLoop);
  }

  void TearDown() override {
    message_loop_.reset();
  }

 protected:
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(WatcherTest, WatchBasic) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);

  bool notified = false;
  base::RunLoop run_loop;
  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            OnReady([&] (MojoResult result) {
                              EXPECT_EQ(MOJO_RESULT_OK, result);
                              notified = true;
                              run_loop.Quit();
                            })));
  EXPECT_TRUE(b_watcher.IsWatching());

  EXPECT_EQ(MOJO_RESULT_OK, WriteMessageRaw(a.get(), "hello", 5, nullptr, 0,
                                            MOJO_WRITE_MESSAGE_FLAG_NONE));
  run_loop.Run();
  EXPECT_TRUE(notified);

  b_watcher.Cancel();
}

TEST_F(WatcherTest, WatchUnsatisfiable) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);
  a.reset();

  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            NotReached()));
  EXPECT_FALSE(b_watcher.IsWatching());
}

TEST_F(WatcherTest, WatchInvalidHandle) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);
  a.reset();
  b.reset();

  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            NotReached()));
  EXPECT_FALSE(b_watcher.IsWatching());
}

TEST_F(WatcherTest, Cancel) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);

  base::RunLoop run_loop;
  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            NotReached()));
  EXPECT_TRUE(b_watcher.IsWatching());
  b_watcher.Cancel();
  EXPECT_FALSE(b_watcher.IsWatching());

  // This should never trigger the watcher.
  EXPECT_EQ(MOJO_RESULT_OK, WriteMessageRaw(a.get(), "hello", 5, nullptr, 0,
                                            MOJO_WRITE_MESSAGE_FLAG_NONE));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WatcherTest, CancelOnClose) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);

  base::RunLoop run_loop;
  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            OnReady([&] (MojoResult result) {
                              EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
                              run_loop.Quit();
                            })));
  EXPECT_TRUE(b_watcher.IsWatching());

  // This should trigger the watcher above.
  b.reset();

  run_loop.Run();

  EXPECT_FALSE(b_watcher.IsWatching());
}

TEST_F(WatcherTest, CancelOnDestruction) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);
  base::RunLoop run_loop;
  {
    Watcher b_watcher;
    EXPECT_EQ(MOJO_RESULT_OK,
              b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                              NotReached()));
    EXPECT_TRUE(b_watcher.IsWatching());

    // |b_watcher| should be cancelled when it goes out of scope.
  }

  // This should never trigger the watcher above.
  EXPECT_EQ(MOJO_RESULT_OK, WriteMessageRaw(a.get(), "hello", 5, nullptr, 0,
                                            MOJO_WRITE_MESSAGE_FLAG_NONE));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WatcherTest, NotifyOnMessageLoopDestruction) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);

  bool notified = false;
  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            OnReady([&] (MojoResult result) {
                              EXPECT_EQ(MOJO_RESULT_ABORTED, result);
                              notified = true;
                            })));
  EXPECT_TRUE(b_watcher.IsWatching());

  message_loop_.reset();

  EXPECT_TRUE(notified);

  EXPECT_TRUE(b_watcher.IsWatching());
  b_watcher.Cancel();
}

TEST_F(WatcherTest, CloseAndCancel) {
  ScopedMessagePipeHandle a, b;
  CreateMessagePipe(nullptr, &a, &b);

  Watcher b_watcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            b_watcher.Start(b.get(), MOJO_HANDLE_SIGNAL_READABLE,
                            OnReady([](MojoResult result) { FAIL(); })));
  EXPECT_TRUE(b_watcher.IsWatching());

  // This should trigger the watcher above...
  b.reset();
  // ...but the watcher is cancelled first.
  b_watcher.Cancel();

  EXPECT_FALSE(b_watcher.IsWatching());

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace mojo
