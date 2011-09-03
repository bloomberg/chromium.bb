// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/watching_file_reader.h"

#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class WatchingFileReaderTest : public testing::Test {
 public:

  // The class under test
  class TestWatchingFileReader : public WatchingFileReader {
   public:
    explicit TestWatchingFileReader(WatchingFileReaderTest* t)
      : test_(t) {}
    virtual void DoRead() OVERRIDE {
      ASSERT_TRUE(test_);
      test_->OnRead();
    }
    virtual void OnReadFinished() OVERRIDE {
      ASSERT_TRUE(test_);
      test_->OnReadFinished();
    }
    void TestCancel() {
      // Could execute concurrently with DoRead()
      test_ = NULL;
    }
   private:
    virtual ~TestWatchingFileReader() {}
    WatchingFileReaderTest* test_;
    base::Lock lock_;
  };

  // Mocks

  // WatcherDelegate owns the FilePathWatcherFactory it gets, so use simple
  // proxies to call the test fixture.

  class MockFilePathWatcherShim
    : public FilePathWatcherShim {
   public:
    typedef base::files::FilePathWatcher::Delegate Delegate;

    explicit MockFilePathWatcherShim(WatchingFileReaderTest* t) : test_(t) {}
    virtual ~MockFilePathWatcherShim() {
      test_->OnShimDestroyed(this);
    }

    // Enforce one-Watch-per-lifetime as the original FilePathWatcher
    virtual bool Watch(const FilePath& path,
                       Delegate* delegate) OVERRIDE {
      EXPECT_TRUE(path_.empty()) << "Only one-Watch-per-lifetime allowed.";
      EXPECT_TRUE(!delegate_.get());
      path_ = path;
      delegate_ = delegate;
      return test_->OnWatch();
    }

    void PathChanged() {
      delegate_->OnFilePathChanged(path_);
    }

    void PathError() {
      delegate_->OnFilePathError(path_);
    }

   private:
    FilePath path_;
    scoped_refptr<Delegate> delegate_;
    WatchingFileReaderTest* test_;
  };

  class MockFilePathWatcherFactory
    : public FilePathWatcherFactory {
   public:
    explicit MockFilePathWatcherFactory(WatchingFileReaderTest* t)
      : test(t) {}
    virtual FilePathWatcherShim*
        CreateFilePathWatcher() OVERRIDE {
      return test->CreateFilePathWatcher();
    }
    WatchingFileReaderTest* test;
  };

  // Helpers for mocks.

  FilePathWatcherShim* CreateFilePathWatcher() {
    watcher_shim_ = new MockFilePathWatcherShim(this);
    return watcher_shim_;
  }

  void OnShimDestroyed(MockFilePathWatcherShim* destroyed_shim) {
    // Precaution to avoid segfault.
    if (watcher_shim_ == destroyed_shim)
      watcher_shim_ = NULL;
  }

  // On each event, post QuitTask to allow use of MessageLoop::Run() to
  // synchronize the threads.

  bool OnWatch() {
    EXPECT_TRUE(message_loop_ == MessageLoop::current());
    BreakNow("OnWatch");
    return !fail_on_watch_;
  }

  void OnRead() {
    { // Check that OnRead is executed serially.
      base::AutoLock lock(read_lock_);
      EXPECT_FALSE(read_running_) << "DoRead is not called serially!";
      read_running_ = true;
    }
    BreakNow("OnRead");
    read_allowed_.Wait();
    // Calling from WorkerPool, but protected by read_allowed_/read_called_.
    output_value_ = input_value_;

    { // This lock might be destroyed after read_called_ is signalled.
      base::AutoLock lock(read_lock_);
      read_running_ = false;
    }
    read_called_.Signal();
  }

  void OnReadFinished() {
    EXPECT_TRUE(message_loop_ == MessageLoop::current());
    EXPECT_EQ(output_value_, input_value_);
    BreakNow("OnReadFinished");
  }

 protected:
  friend class BreakTask;
  class BreakTask : public Task {
   public:
    BreakTask(WatchingFileReaderTest* test, std::string breakpoint)
      : test_(test), breakpoint_(breakpoint) {}
    virtual ~BreakTask() {}
    virtual void Run() OVERRIDE {
      test_->breakpoint_ = breakpoint_;
      MessageLoop::current()->QuitNow();
    }
   private:
    WatchingFileReaderTest* test_;
    std::string breakpoint_;
  };

  void BreakNow(std::string b) {
    message_loop_->PostTask(FROM_HERE, new BreakTask(this, b));
  }

  void RunUntilBreak(std::string b) {
    message_loop_->Run();
    ASSERT_EQ(breakpoint_, b);
  }

  WatchingFileReaderTest()
      : watcher_shim_(NULL),
        fail_on_watch_(false),
        input_value_(0),
        output_value_(-1),
        read_allowed_(false, false),
        read_called_(false, false),
        read_running_(false) {
  }

  // Helpers for tests.

  // Lets OnRead run and waits for it to complete. Can only return if OnRead is
  // executed on a concurrent thread.
  void WaitForRead() {
    RunUntilBreak("OnRead");
    read_allowed_.Signal();
    read_called_.Wait();
  }

  // test::Test methods
  virtual void SetUp() OVERRIDE {
    message_loop_ = MessageLoop::current();
    watcher_ = new TestWatchingFileReader(this);
    watcher_->set_watcher_factory(new MockFilePathWatcherFactory(this));
  }

  virtual void TearDown() OVERRIDE {
    // Cancel the watcher to catch if it makes a late DoRead call.
    watcher_->TestCancel();
    watcher_->Cancel();
    // Check if OnRead is stalled.
    EXPECT_FALSE(read_running_) << "OnRead should be done by TearDown";
    // Release it for cleanliness.
    if (read_running_) {
      WaitForRead();
    }
  }

  MockFilePathWatcherShim* watcher_shim_;

  bool fail_on_watch_;

  // Input value read on WorkerPool.
  int input_value_;
  // Output value written on WorkerPool.
  int output_value_;

  // read is called on WorkerPool so we need to synchronize with it.
  base::WaitableEvent read_allowed_;
  base::WaitableEvent read_called_;

  // Protected by read_lock_. Used to verify that read calls are serialized.
  bool read_running_;
  base::Lock read_lock_;

  // Loop for this thread.
  MessageLoop* message_loop_;

  // WatcherDelegate under test.
  scoped_refptr<TestWatchingFileReader> watcher_;

  std::string breakpoint_;
};

TEST_F(WatchingFileReaderTest, FilePathWatcherFailures) {
  fail_on_watch_ = true;
  watcher_->StartWatch(FilePath(FILE_PATH_LITERAL("some_file_name")));
  RunUntilBreak("OnWatch");

  fail_on_watch_ = false;
  RunUntilBreak("OnWatch");  // Due to backoff this will take 100ms.
  WaitForRead();
  RunUntilBreak("OnReadFinished");

  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathError();
  RunUntilBreak("OnWatch");
  WaitForRead();
  RunUntilBreak("OnReadFinished");

  message_loop_->AssertIdle();
}

TEST_F(WatchingFileReaderTest, ExecuteAndSerializeReads) {
  watcher_->StartWatch(FilePath(FILE_PATH_LITERAL("some_file_name")));
  RunUntilBreak("OnWatch");
  WaitForRead();
  RunUntilBreak("OnReadFinished");

  message_loop_->AssertIdle();

  ++input_value_;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  WaitForRead();
  RunUntilBreak("OnReadFinished");

  message_loop_->AssertIdle();

  ++input_value_;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  WaitForRead();
  RunUntilBreak("OnReadFinished");

  message_loop_->AssertIdle();

  // Schedule two calls. OnRead checks if it is called serially.
  ++input_value_;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  // read is blocked, so this will have to induce read_pending
  watcher_shim_->PathChanged();
  WaitForRead();
  WaitForRead();
  RunUntilBreak("OnReadFinished");

  // No more tasks should remain.
  message_loop_->AssertIdle();
}

}  // namespace

}  // namespace net

