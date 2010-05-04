// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/thread_blocker.h"

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
class Flag;
};  // namespace browser_sync

// We manage the lifetime of browser_sync::Flag ourselves.
template <>
struct RunnableMethodTraits<browser_sync::Flag> {
  void RetainCallee(browser_sync::Flag*) {}
  void ReleaseCallee(browser_sync::Flag*) {}
};

namespace browser_sync {

// Utility class that is basically just a thread-safe boolean.
class Flag {
 public:
  Flag() : flag_(false) {}

  bool IsSet() const {
    AutoLock auto_lock(lock_);
    return flag_;
  }

  void Set() {
    AutoLock auto_lock(lock_);
    flag_ = true;
  }

  void Unset() {
    AutoLock auto_lock(lock_);
    flag_ = false;
  }

 private:
  mutable Lock lock_;
  bool flag_;

  DISALLOW_COPY_AND_ASSIGN(Flag);
};

namespace {

class ThreadBlockerTest : public testing::Test {
 protected:
  ThreadBlockerTest() : target_thread_("Target Thread") {}

  virtual ~ThreadBlockerTest() {
    CHECK(!thread_blocker_.get());
  }

  virtual void SetUp() {
    CHECK(target_thread_.Start());
    thread_blocker_.reset(new ThreadBlocker(&target_thread_));
  }

  virtual void TearDown() {
    target_thread_.Stop();
    thread_blocker_.reset();
  }

  base::Thread target_thread_;
  scoped_ptr<ThreadBlocker> thread_blocker_;
  Flag flag_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadBlockerTest);
};

TEST_F(ThreadBlockerTest, Basic) {
  thread_blocker_->Block();
  target_thread_.message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(&flag_, &Flag::Set));
  EXPECT_FALSE(flag_.IsSet());
  thread_blocker_->Unblock();
  // Need to block again to make sure this thread waits for the posted
  // method to run.
  thread_blocker_->Block();
  EXPECT_TRUE(flag_.IsSet());
  thread_blocker_->Unblock();
}

TEST_F(ThreadBlockerTest, SetUnset) {
  thread_blocker_->Block();
  target_thread_.message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(&flag_, &Flag::Set));
  target_thread_.message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(&flag_, &Flag::Unset));
  EXPECT_FALSE(flag_.IsSet());
  thread_blocker_->Unblock();
  // Need to block again here too.
  thread_blocker_->Block();
  EXPECT_FALSE(flag_.IsSet());
  thread_blocker_->Unblock();
}

}  // namespace

}  // namespace browser_sync
