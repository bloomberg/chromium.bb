// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class ChromeThreadTest : public testing::Test {
 public:
  void Release() {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

 protected:
  virtual void SetUp() {
    ui_thread_.reset(new ChromeThread(ChromeThread::UI));
    file_thread_.reset(new ChromeThread(ChromeThread::FILE));
    ui_thread_->Start();
    file_thread_->Start();
  }

  virtual void TearDown() {
    ui_thread_->Stop();
    file_thread_->Stop();
  }

  static void BasicFunction(MessageLoop* message_loop) {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
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
            DeletedOnFile, ChromeThread::DeleteOnFileThread> {
   public:
    explicit DeletedOnFile(MessageLoop* message_loop)
        : message_loop_(message_loop) { }

    ~DeletedOnFile() {
      CHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
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
  scoped_ptr<ChromeThread> ui_thread_;
  scoped_ptr<ChromeThread> file_thread_;
  MessageLoop loop_;
};

TEST_F(ChromeThreadTest, PostTask) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(&BasicFunction, MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(ChromeThreadTest, Release) {
  ChromeThread::ReleaseSoon(ChromeThread::UI, FROM_HERE, this);
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
    scoped_refptr<DeletedOnFile> test(
        new DeletedOnFile(MessageLoop::current()));
  }
  MessageLoop::current()->Run();
}

TEST_F(ChromeThreadTest, NotReleasedIfTargetThreadNonExistent) {
  scoped_refptr<NeverDeleted> test(new NeverDeleted());
}

TEST_F(ChromeThreadTest, PostTaskViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE);
  message_loop_proxy->PostTask(FROM_HERE,
                               NewRunnableFunction(&BasicFunction,
                                                   MessageLoop::current()));
  MessageLoop::current()->Run();
}

TEST_F(ChromeThreadTest, ReleaseViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::UI);
  message_loop_proxy->ReleaseSoon(FROM_HERE, this);
  MessageLoop::current()->Run();
}

TEST_F(ChromeThreadTest, TaskToNonExistentThreadIsDeletedViaMessageLoopProxy) {
  bool deleted = false;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::WEBKIT);
  message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_TRUE(deleted);
}

TEST_F(ChromeThreadTest, PostTaskViaMessageLoopProxyAfterThreadExits) {
  scoped_ptr<ChromeThread> io_thread(new ChromeThread(ChromeThread::IO));
  io_thread->Start();
  io_thread->Stop();

  bool deleted = false;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::IO);
  bool ret = message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_FALSE(ret);
  EXPECT_TRUE(deleted);
}

TEST_F(ChromeThreadTest, PostTaskViaMessageLoopProxyAfterThreadIsDeleted) {
  {
    scoped_ptr<ChromeThread> io_thread(new ChromeThread(ChromeThread::IO));
    io_thread->Start();
  }
  bool deleted = false;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::IO);
  bool ret = message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_FALSE(ret);
  EXPECT_TRUE(deleted);
}

