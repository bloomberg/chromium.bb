// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/object_watcher.h"

#include <process.h>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

class QuitDelegate : public ObjectWatcher::Delegate {
 public:
  void OnObjectSignaled(HANDLE object) override {
    MessageLoop::current()->QuitWhenIdle();
  }
};

class DecrementCountDelegate : public ObjectWatcher::Delegate {
 public:
  explicit DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  void OnObjectSignaled(HANDLE object) override { --(*counter_); }

 private:
  int* counter_;
};

void RunTest_BasicSignal(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  ObjectWatcher watcher;
  EXPECT_FALSE(watcher.IsWatching());

  // A manual-reset event that is not yet signaled.
  base::win::ScopedHandle event(CreateEvent(NULL, TRUE, FALSE, NULL));

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event.Get(), &delegate);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_EQ(event.Get(), watcher.GetWatchedObject());

  SetEvent(event.Get());

  MessageLoop::current()->Run();

  EXPECT_FALSE(watcher.IsWatching());
}

void RunTest_BasicCancel(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  base::win::ScopedHandle event(CreateEvent(NULL, TRUE, FALSE, NULL));

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event.Get(), &delegate);
  EXPECT_TRUE(ok);

  watcher.StopWatching();
}

void RunTest_CancelAfterSet(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  ObjectWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

  // A manual-reset event that is not yet signaled.
  base::win::ScopedHandle event(CreateEvent(NULL, TRUE, FALSE, NULL));

  bool ok = watcher.StartWatching(event.Get(), &delegate);
  EXPECT_TRUE(ok);

  SetEvent(event.Get());

  // Let the background thread do its business
  Sleep(30);

  watcher.StopWatching();

  RunLoop().RunUntilIdle();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);
}

void RunTest_SignalBeforeWatch(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  ObjectWatcher watcher;

  // A manual-reset event that is signaled before we begin watching.
  base::win::ScopedHandle event(CreateEvent(NULL, TRUE, TRUE, NULL));

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event.Get(), &delegate);
  EXPECT_TRUE(ok);

  MessageLoop::current()->Run();

  EXPECT_FALSE(watcher.IsWatching());
}

void RunTest_OutlivesMessageLoop(MessageLoop::Type message_loop_type) {
  // Simulate a MessageLoop that dies before an ObjectWatcher.  This ordinarily
  // doesn't happen when people use the Thread class, but it can happen when
  // people use the Singleton pattern or atexit.
  // Note that |event| is not signaled
  base::win::ScopedHandle event(CreateEvent(NULL, TRUE, FALSE, NULL));
  {
    ObjectWatcher watcher;
    {
      MessageLoop message_loop(message_loop_type);

      QuitDelegate delegate;
      watcher.StartWatching(event.Get(), &delegate);
    }
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TEST(ObjectWatcherTest, BasicSignal) {
  RunTest_BasicSignal(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicSignal(MessageLoop::TYPE_IO);
  RunTest_BasicSignal(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, BasicCancel) {
  RunTest_BasicCancel(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicCancel(MessageLoop::TYPE_IO);
  RunTest_BasicCancel(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, CancelAfterSet) {
  RunTest_CancelAfterSet(MessageLoop::TYPE_DEFAULT);
  RunTest_CancelAfterSet(MessageLoop::TYPE_IO);
  RunTest_CancelAfterSet(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, SignalBeforeWatch) {
  RunTest_SignalBeforeWatch(MessageLoop::TYPE_DEFAULT);
  RunTest_SignalBeforeWatch(MessageLoop::TYPE_IO);
  RunTest_SignalBeforeWatch(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, OutlivesMessageLoop) {
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_DEFAULT);
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_IO);
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_UI);
}

}  // namespace win
}  // namespace base
