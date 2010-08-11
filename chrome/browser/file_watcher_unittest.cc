// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_watcher.h"

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;

#if defined(OS_MACOSX)
// TODO(tony): Tests are flaky on mac.  http://crbug.com/38188
#define MAYBE(name) FLAKY_ ## name
#else
#define MAYBE(name) name
#endif

namespace {

// For tests where we wait a bit to verify nothing happened
const int kWaitForEventTime = 500;

// Maximum amount of time to wait on a test.
const int kMaxTestTimeMs = 10 * 1000;

// A mock FileWatcher::Delegate for testing.
class TestDelegate : public FileWatcher::Delegate {
 public:
  MOCK_METHOD1(OnFileChanged, void(const FilePath&));
};

class FileWatcherTest : public testing::Test {
 public:
  // Implementation of FileWatcher on Mac requires UI loop.
  FileWatcherTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(ChromeThread::UI, &loop_),
        file_thread_(ChromeThread::FILE, &loop_) {
  }

 protected:
  virtual void SetUp() {
    temp_dir_.reset(new ScopedTempDir);
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    // Make sure that not getting an event doesn't cause the whole
    // test suite to hang.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kMaxTestTimeMs);
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  FilePath test_file() {
    return temp_dir_->path().AppendASCII("FileWatcherTest");
  }

  // Write |content| to the test file. Returns true on success.
  bool WriteTestFile(const std::string& content) {
    // Logging to try and figure out why these tests are flaky on mac.
    LOG(INFO) << "WriteTestFile";
    int write_size = file_util::WriteFile(test_file(), content.c_str(),
                                          content.length());
    SyncIfPOSIX();
    return write_size == static_cast<int>(content.length());
  }

  void VerifyDelegate(TestDelegate* delegate) {
    // Check that we get at least the expected number of notified delegates.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kWaitForEventTime);
    loop_.Run();
    Mock::VerifyAndClearExpectations(delegate);
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
  ChromeThread ui_thread_;
  ChromeThread file_thread_;
  scoped_ptr<ScopedTempDir> temp_dir_;
};

// Basic test: Create the file and verify that we notice.
TEST_F(FileWatcherTest, MAYBE(NewFile)) {
  FileWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  ASSERT_TRUE(watcher.Watch(test_file(), delegate.get()));

  EXPECT_CALL(*delegate, OnFileChanged(_)).Times(AtLeast(1));
  ASSERT_TRUE(WriteTestFile("content"));
  VerifyDelegate(delegate.get());
}

// Verify that modifying the file is caught.
TEST_F(FileWatcherTest, MAYBE(ModifiedFile)) {
  ASSERT_TRUE(WriteTestFile("content"));

  FileWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  ASSERT_TRUE(watcher.Watch(test_file(), delegate.get()));

  // Now make sure we get notified if the file is modified.
  EXPECT_CALL(*delegate, OnFileChanged(_)).Times(AtLeast(1));
  ASSERT_TRUE(WriteTestFile("new content"));
  VerifyDelegate(delegate.get());
}

TEST_F(FileWatcherTest, MAYBE(DeletedFile)) {
  ASSERT_TRUE(WriteTestFile("content"));

  FileWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  ASSERT_TRUE(watcher.Watch(test_file(), delegate.get()));

  // Now make sure we get notified if the file is deleted.
  EXPECT_CALL(*delegate, OnFileChanged(_)).Times(AtLeast(1));
  file_util::Delete(test_file(), false);
  SyncIfPOSIX();
  VerifyDelegate(delegate.get());
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(FileWatcherTest, MAYBE(Unregister)) {
  scoped_refptr<TestDelegate> delegate(new TestDelegate);

  {
    FileWatcher watcher;
    ASSERT_TRUE(watcher.Watch(test_file(), delegate.get()));

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  EXPECT_CALL(*delegate, OnFileChanged(_)).Times(0);
  ASSERT_TRUE(WriteTestFile("content"));
  VerifyDelegate(delegate.get());
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
TEST_F(FileWatcherTest, MAYBE(DeleteDuringNotify)) {
  FileWatcher* watcher = new FileWatcher;
  // Takes ownership of watcher.
  scoped_refptr<Deleter> deleter(new Deleter(watcher, &loop_));
  ASSERT_TRUE(watcher->Watch(test_file(), deleter.get()));

  ASSERT_TRUE(WriteTestFile("content"));
  loop_.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter->watcher_.get() == NULL);
}

// Verify that deleting the watcher works even if there is a pending
// notification.
//
// It's hard to test this, since both a change event and deletion of the file
// watcher must happen before the task that runs the callback executes. The code
// below only triggers the situation with the Linux implementation. Change
// detection runs on a separate thread in the Linux implementation, so we can
// schedule the FileWatcher deletion in advance. For Mac and Windows, the
// DeleteTask runs before the message loop processes the platform-specific
// change notifications, so the whole FileWatcher is destroyed before the
// callback gets scheduled.
TEST_F(FileWatcherTest, MAYBE(DestroyWithPendingNotification)) {
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  EXPECT_CALL(*delegate, OnFileChanged(_)).Times(AnyNumber());
  FileWatcher* watcher = new FileWatcher;
  ASSERT_TRUE(watcher->Watch(test_file(), delegate.get()));
  ASSERT_TRUE(WriteTestFile("content"));
  ChromeThread::DeleteSoon(ChromeThread::FILE, FROM_HERE, watcher);
  // Success if there is no crash or DCHECK when running the callback.
  VerifyDelegate(delegate.get());
}

TEST_F(FileWatcherTest, MAYBE(MultipleWatchersSingleFile)) {
  FileWatcher watcher1, watcher2;
  scoped_refptr<TestDelegate> delegate1(new TestDelegate);
  scoped_refptr<TestDelegate> delegate2(new TestDelegate);
  ASSERT_TRUE(watcher1.Watch(test_file(), delegate1.get()));
  ASSERT_TRUE(watcher2.Watch(test_file(), delegate2.get()));

  EXPECT_CALL(*delegate1, OnFileChanged(_)).Times(AtLeast(1));
  EXPECT_CALL(*delegate2, OnFileChanged(_)).Times(AtLeast(1));
  ASSERT_TRUE(WriteTestFile("content"));
  VerifyDelegate(delegate1.get());
  VerifyDelegate(delegate2.get());
}

// Verify that watching a file who's parent directory doesn't exist
// fails, but doesn't asssert.
TEST_F(FileWatcherTest, NonExistentDirectory) {
  FileWatcher watcher;
  ASSERT_FALSE(watcher.Watch(test_file().AppendASCII("FileToWatch"), NULL));
}

}  // namespace
