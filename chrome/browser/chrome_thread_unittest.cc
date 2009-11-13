// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class ChromeThreadTest : public testing::Test {
 public:
  void Release() {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
    loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

 protected:
  virtual void SetUp() {
    file_thread_.reset(new ChromeThread(ChromeThread::FILE));
    io_thread_.reset(new ChromeThread(ChromeThread::IO));
    file_thread_->Start();
    io_thread_->Start();
  }

  virtual void TearDown() {
    file_thread_->Stop();
    io_thread_->Stop();
  }

  static void BasicFunction(MessageLoop* message_loop) {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

  class DummyTask : public Task {
   public:
    explicit DummyTask(bool* deleted) : deleted_(deleted) { }
    ~DummyTask() {
      *deleted_ = true;
    }

    void Run() {
      CHECK(false);
    }

   private:
    bool* deleted_;
  };

  class DeletedOnIO
      : public base::RefCountedThreadSafe<
            DeletedOnIO, ChromeThread::DeleteOnIOThread> {
   public:
    explicit DeletedOnIO(MessageLoop* message_loop)
        : message_loop_(message_loop) { }

    ~DeletedOnIO() {
      CHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
      message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    }

   private:
    MessageLoop* message_loop_;
  };

  class NeverDeleted
      : public base::RefCountedThreadSafe<
            NeverDeleted, ChromeThread::DeleteOnWebKitThread> {
   public:
    ~NeverDeleted() {
      CHECK(false);
    }
  };

 private:
  scoped_ptr<ChromeThread> file_thread_;
  scoped_ptr<ChromeThread> io_thread_;
  MessageLoop loop_;
};

TEST_F(ChromeThreadTest, PostTask) {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&BasicFunction, MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(ChromeThreadTest, Release) {
  ChromeThread::ReleaseSoon(ChromeThread::FILE, FROM_HERE, this);
  MessageLoop::current()->Run();
}

TEST_F(ChromeThreadTest, TaskToNonExistentThreadIsDeleted) {
  bool deleted = false;
  ChromeThread::PostTask(
      ChromeThread::WEBKIT, FROM_HERE,
      new DummyTask(&deleted));
  EXPECT_TRUE(deleted);
}

TEST_F(ChromeThreadTest, ReleasedOnCorrectThread) {
  {
    scoped_refptr<DeletedOnIO> test(new DeletedOnIO(MessageLoop::current()));
  }
  MessageLoop::current()->Run();
}

// TODO(jam): add proper purify suppression so this test doesn't fail.
// http://crbug.com/27630
TEST_F(ChromeThreadTest, DISABLED_NotReleasedIfTargetThreadNonExistent) {
  scoped_refptr<NeverDeleted> test(new NeverDeleted());
}
