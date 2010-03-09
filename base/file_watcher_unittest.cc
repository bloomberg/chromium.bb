// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_watcher.h"

#include <limits>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/thread.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif  // defined(OS_WIN)
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// For tests where we wait a bit to verify nothing happened
const int kWaitForEventTime = 500;

class FileWatcherTest : public testing::Test {
 public:
  // Implementation of FileWatcher on Mac requires UI loop.
  FileWatcherTest()
      : loop_(MessageLoop::TYPE_UI),
        notified_delegates_(0),
        expected_notified_delegates_(0) {
  }

  void OnTestDelegateFirstNotification() {
    notified_delegates_++;
    if (notified_delegates_ >= expected_notified_delegates_)
      MessageLoop::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    temp_dir_.reset(new ScopedTempDir);
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
  }

  FilePath test_file() {
    return temp_dir_->path().AppendASCII("FileWatcherTest");
  }

  virtual void TearDown() {
    // Make sure there are no tasks in the loop.
    loop_.RunAllPending();
  }

  // Write |content| to the test file. Returns true on success.
  bool WriteTestFile(const std::string& content) {
    int write_size = file_util::WriteFile(test_file(), content.c_str(),
                                          content.length());
    return write_size == static_cast<int>(content.length());
  }

  void SetExpectedNumberOfNotifiedDelegates(int n) {
    notified_delegates_ = 0;
    expected_notified_delegates_ = n;
  }

  void VerifyExpectedNumberOfNotifiedDelegates() {
    // Check that we get at least the expected number of notified delegates.
    if (expected_notified_delegates_ - notified_delegates_ > 0)
      loop_.Run();
    EXPECT_EQ(expected_notified_delegates_, notified_delegates_);
  }

  void VerifyNoExtraNotifications() {
    // Check that we get no more than the expected number of notified delegates.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kWaitForEventTime);
    loop_.Run();
    EXPECT_EQ(expected_notified_delegates_, notified_delegates_);
  }

  // We need this function for reliable tests on Mac OS X. FSEvents API
  // has a latency interval and can merge multiple events into one,
  // and we need a clear distinction between events triggered by test setup code
  // and test code.
  void SyncIfPOSIX() {
#if defined(OS_POSIX)
    sync();
#endif  // defined(OS_POSIX)
  }

  MessageLoop loop_;
  scoped_ptr<ScopedTempDir> temp_dir_;

  // The number of test delegates which received their notification.
  int notified_delegates_;

  // The number of notified test delegates after which we quit the message loop.
  int expected_notified_delegates_;
};

class TestDelegate : public FileWatcher::Delegate {
 public:
  explicit TestDelegate(FileWatcherTest* test)
      : test_(test),
        got_notification_(false),
        original_thread_id_(PlatformThread::CurrentId()) {
  }

  bool got_notification() const {
    return got_notification_;
  }

  void reset() {
    got_notification_ = false;
  }

  virtual void OnFileChanged(const FilePath& path) {
    EXPECT_EQ(original_thread_id_, PlatformThread::CurrentId());
    if (!got_notification_)
      test_->OnTestDelegateFirstNotification();
    got_notification_ = true;
  }

 private:
  // Hold a pointer to current test fixture to inform it on first notification.
  FileWatcherTest* test_;

  // Set to true after first notification.
  bool got_notification_;

  // Keep track of original thread id to verify that callbacks are called
  // on the same thread.
  PlatformThreadId original_thread_id_;
};

// Basic test: Create the file and verify that we notice.
TEST_F(FileWatcherTest, NewFile) {
  FileWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_file(), &delegate, NULL));

  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile("content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that modifying the file is caught.
TEST_F(FileWatcherTest, ModifiedFile) {
  ASSERT_TRUE(WriteTestFile("content"));
  SyncIfPOSIX();

  FileWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_file(), &delegate, NULL));

  // Now make sure we get notified if the file is modified.
  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile("new content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(FileWatcherTest, DeletedFile) {
  ASSERT_TRUE(WriteTestFile("content"));
  SyncIfPOSIX();

  FileWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_file(), &delegate, NULL));

  // Now make sure we get notified if the file is deleted.
  SetExpectedNumberOfNotifiedDelegates(1);
  file_util::Delete(test_file(), false);
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(FileWatcherTest, Unregister) {
  TestDelegate delegate(this);

  {
    FileWatcher watcher;
    ASSERT_TRUE(watcher.Watch(test_file(), &delegate, NULL));

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  SetExpectedNumberOfNotifiedDelegates(0);
  ASSERT_TRUE(WriteTestFile("content"));
  VerifyExpectedNumberOfNotifiedDelegates();
  VerifyNoExtraNotifications();
}


namespace {
// Used by the DeleteDuringNotify test below.
// Deletes the FileWatcher when it's notified.
class Deleter : public FileWatcher::Delegate {
 public:
  Deleter(FileWatcher* watcher, MessageLoop* loop)
      : watcher_(watcher),
        loop_(loop) {
  }

  virtual void OnFileChanged(const FilePath& path) {
    watcher_.reset(NULL);
    loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  scoped_ptr<FileWatcher> watcher_;
  MessageLoop* loop_;
};
}  // anonymous namespace

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FileWatcherTest, DeleteDuringNotify) {
  FileWatcher* watcher = new FileWatcher;
  Deleter deleter(watcher, &loop_);  // Takes ownership of watcher.
  ASSERT_TRUE(watcher->Watch(test_file(), &deleter, NULL));

  ASSERT_TRUE(WriteTestFile("content"));
  loop_.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter.watcher_.get() == NULL);
}

TEST_F(FileWatcherTest, BackendLoop) {
  base::Thread thread("test");
  ASSERT_TRUE(thread.Start());

  FileWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_file(), &delegate, thread.message_loop()));
}

TEST_F(FileWatcherTest, MultipleWatchersSingleFile) {
  FileWatcher watcher1, watcher2;
  TestDelegate delegate1(this), delegate2(this);
  ASSERT_TRUE(watcher1.Watch(test_file(), &delegate1, NULL));
  ASSERT_TRUE(watcher2.Watch(test_file(), &delegate2, NULL));

  SetExpectedNumberOfNotifiedDelegates(2);
  ASSERT_TRUE(WriteTestFile("content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that watching a file who's parent directory doesn't exist
// fails, but doesn't asssert.
TEST_F(FileWatcherTest, NonExistentDirectory) {
  FileWatcher watcher;
  ASSERT_FALSE(watcher.Watch(test_file().AppendASCII("FileToWatch"),
                             NULL, NULL));
}

}  // namespace
