// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

 protected:
  virtual void SetUp() {
    ui_thread_.reset(new BrowserThread(BrowserThread::UI));
    file_thread_.reset(new BrowserThread(BrowserThread::FILE));
    ui_thread_->Start();
    file_thread_->Start();
  }

  virtual void TearDown() {
    ui_thread_->Stop();
    file_thread_->Stop();
  }

  static void BasicFunction(MessageLoop* message_loop) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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

  class DeletedOnFile
      : public base::RefCountedThreadSafe<
            DeletedOnFile, BrowserThread::DeleteOnFileThread> {
   public:
    explicit DeletedOnFile(MessageLoop* message_loop)
        : message_loop_(message_loop) { }

    ~DeletedOnFile() {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
      message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    }

   private:
    MessageLoop* message_loop_;
  };

  class NeverDeleted
      : public base::RefCountedThreadSafe<
            NeverDeleted, BrowserThread::DeleteOnWebKitThread> {
   public:
    ~NeverDeleted() {
      CHECK(false);
    }
  };

 private:
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> file_thread_;
  // It's kind of ugly to make this mutable - solely so we can post the Quit
  // Task from Release(). This should be fixed.
  mutable MessageLoop loop_;
};

TEST_F(BrowserThreadTest, PostTask) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&BasicFunction, MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, Release) {
  BrowserThread::ReleaseSoon(BrowserThread::UI, FROM_HERE, this);
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, TaskToNonExistentThreadIsDeleted) {
  bool deleted = false;
  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
      new DummyTask(&deleted));
  EXPECT_TRUE(deleted);
}

TEST_F(BrowserThreadTest, ReleasedOnCorrectThread) {
  {
    scoped_refptr<DeletedOnFile> test(
        new DeletedOnFile(MessageLoop::current()));
  }
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, NotReleasedIfTargetThreadNonExistent) {
  scoped_refptr<NeverDeleted> test(new NeverDeleted());
}

TEST_F(BrowserThreadTest, PostTaskViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  message_loop_proxy->PostTask(FROM_HERE,
                               NewRunnableFunction(&BasicFunction,
                                                   MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, ReleaseViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  message_loop_proxy->ReleaseSoon(FROM_HERE, this);
  MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, TaskToNonExistentThreadIsDeletedViaMessageLoopProxy) {
  bool deleted = false;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT);
  message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_TRUE(deleted);
}

TEST_F(BrowserThreadTest, PostTaskViaMessageLoopProxyAfterThreadExits) {
  scoped_ptr<BrowserThread> io_thread(new BrowserThread(BrowserThread::IO));
  io_thread->Start();
  io_thread->Stop();

  bool deleted = false;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  bool ret = message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_FALSE(ret);
  EXPECT_TRUE(deleted);
}

TEST_F(BrowserThreadTest, PostTaskViaMessageLoopProxyAfterThreadIsDeleted) {
  {
    scoped_ptr<BrowserThread> io_thread(new BrowserThread(BrowserThread::IO));
    io_thread->Start();
  }
  bool deleted = false;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  bool ret = message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_FALSE(ret);
  EXPECT_TRUE(deleted);
}

