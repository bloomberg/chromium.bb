// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/watching_file_reader.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class WatchingFileReaderTest : public testing::Test {
 public:

  // Mocks

  class MockWorker : public SerialWorker {
   public:
    explicit MockWorker(WatchingFileReaderTest* t) : test_(t) {}
    virtual void WorkNow() OVERRIDE {
      test_->OnWork();
    }
    virtual void DoWork() OVERRIDE {}
    virtual void OnWorkFinished() OVERRIDE {}
   private:
    virtual ~MockWorker() {}
    WatchingFileReaderTest* test_;
  };

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

  void OnWork() {
    EXPECT_TRUE(message_loop_ == MessageLoop::current());
    BreakNow("OnWork");
  }

 protected:
  void BreakCallback(std::string breakpoint) {
    breakpoint_ = breakpoint;
    MessageLoop::current()->QuitNow();
  }

  void BreakNow(std::string b) {
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&WatchingFileReaderTest::BreakCallback,
                   base::Unretained(this), b));
  }

  void RunUntilBreak(std::string b) {
    message_loop_->Run();
    ASSERT_EQ(breakpoint_, b);
  }

  WatchingFileReaderTest()
      : watcher_shim_(NULL),
        fail_on_watch_(false),
        message_loop_(MessageLoop::current()),
        watcher_(new WatchingFileReader()) {
    watcher_->set_watcher_factory(new MockFilePathWatcherFactory(this));
  }

  MockFilePathWatcherShim* watcher_shim_;
  bool fail_on_watch_;
  MessageLoop* message_loop_;
  scoped_ptr<WatchingFileReader> watcher_;

  std::string breakpoint_;
};

TEST_F(WatchingFileReaderTest, FilePathWatcherFailures) {
  fail_on_watch_ = true;
  watcher_->StartWatch(FilePath(FILE_PATH_LITERAL("some_file_name")),
                       new MockWorker(this));
  RunUntilBreak("OnWatch");

  fail_on_watch_ = false;
  RunUntilBreak("OnWatch");  // Due to backoff this will take 100ms.
  RunUntilBreak("OnWork");

  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathError();
  RunUntilBreak("OnWatch");
  RunUntilBreak("OnWork");

  message_loop_->AssertIdle();

  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  RunUntilBreak("OnWork");

  message_loop_->AssertIdle();
}

}  // namespace

}  // namespace net

