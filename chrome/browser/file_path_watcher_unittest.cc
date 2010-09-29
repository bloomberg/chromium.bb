// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_path_watcher.h"

#include <limits>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
// TODO(mnissler): There are flakes on Mac (http://crbug.com/54822) at least for
// FilePathWatcherTest.MultipleWatchersSingleFile.
#define MAYBE(name) FLAKY_ ## name
#else
#define MAYBE(name) name
#endif

namespace {

// The time we wait for events to happen. It should be large enough to be
// reasonably sure all notifications sent in response to file system
// modifications made by the test are processed. This must also accomodate for
// the latency interval we pass to the Mac FSEvents API.
const int kWaitForEventTime = 1000;

// A mock FilePathWatcher::Delegate for testing. I'd rather use gmock, but it's
// not thread safe for setting expectations, so the test code couldn't safely
// reset expectations while the file watcher is running. In order to allow this,
// we implement simple thread safe call counters in TestDelegate.
class TestDelegate : public FilePathWatcher::Delegate {
 public:
  TestDelegate()
      : change_count_(0),
        error_count_(0) {}

  virtual void OnFilePathChanged(const FilePath&) {
    AutoLock lock(lock_);
    ++change_count_;
  }

  virtual void OnError() {
    AutoLock lock(lock_);
    ++error_count_;
  }

  void GetCountsAndReset(int* changes, int* errors) {
    AutoLock lock(lock_);
    *errors = error_count_;
    *changes = change_count_;
    error_count_ = 0;
    change_count_ = 0;
  }

 private:
  Lock lock_;
  int change_count_;
  int error_count_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

// A helper class for setting up watches on the file thread.
class SetupWatchTask : public Task {
 public:
  SetupWatchTask(const FilePath& target,
                 FilePathWatcher* watcher,
                 FilePathWatcher::Delegate* delegate,
                 bool* result,
                 base::WaitableEvent* completion)
      : target_(target),
        watcher_(watcher),
        delegate_(delegate),
        result_(result),
        completion_(completion) {}

  void Run() {
    *result_ = watcher_->Watch(target_, delegate_);
    completion_->Signal();
  }

 private:
  const FilePath target_;
  FilePathWatcher* watcher_;
  FilePathWatcher::Delegate* delegate_;
  bool* result_;
  base::WaitableEvent* completion_;

  DISALLOW_COPY_AND_ASSIGN(SetupWatchTask);
};

class FilePathWatcherTest : public testing::Test {
 public:
  // Implementation of FilePathWatcher on Mac requires UI loop.
  FilePathWatcherTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(ChromeThread::UI, &loop_) {
  }

 protected:
  virtual void SetUp() {
    // Create a separate file thread in order to test proper thread usage.
    file_thread_.reset(new ChromeThread(ChromeThread::FILE));
    file_thread_->Start();
    temp_dir_.reset(new ScopedTempDir);
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
  }

  virtual void TearDown() {
    loop_.RunAllPending();
    file_thread_.reset();
  }

  FilePath test_file() {
    return temp_dir_->path().AppendASCII("FilePathWatcherTest");
  }

  // Write |content| to |file|. Returns true on success.
  bool WriteFile(const FilePath& file, const std::string& content) {
    int write_size = file_util::WriteFile(file, content.c_str(),
                                          content.length());
    SyncIfPOSIX();
    return write_size == static_cast<int>(content.length());
  }

  void SetupWatch(const FilePath& target,
                  FilePathWatcher* watcher,
                  FilePathWatcher::Delegate* delegate) {
    base::WaitableEvent completion(false, false);
    bool result;
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
         new SetupWatchTask(target, watcher, delegate, &result, &completion));
    completion.Wait();
    ASSERT_TRUE(result);
  }

  int WaitForEvents(TestDelegate* delegate) {
    // Wait for events and check the expectations of the delegate.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kWaitForEventTime);
    loop_.Run();
    int errors;
    int changes;
    delegate->GetCountsAndReset(&changes, &errors);
    EXPECT_EQ(0, errors);
    return changes;
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
  scoped_ptr<ChromeThread> file_thread_;
  scoped_ptr<ScopedTempDir> temp_dir_;
};

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, MAYBE(NewFile)) {
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(test_file(), &watcher, delegate.get());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

// Verify that modifying the file is caught.
TEST_F(FilePathWatcherTest, MAYBE(ModifiedFile)) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(test_file(), &watcher, delegate.get());

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

// Verify that moving the file into place is caught.
TEST_F(FilePathWatcherTest, MAYBE(MovedFile)) {
  FilePath source_file(temp_dir_->path().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(test_file(), &watcher, delegate.get());

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(file_util::Move(source_file, test_file()));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

TEST_F(FilePathWatcherTest, MAYBE(DeletedFile)) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(test_file(), &watcher, delegate.get());

  // Now make sure we get notified if the file is deleted.
  file_util::Delete(test_file(), false);
  SyncIfPOSIX();
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(FilePathWatcherTest, Unregister) {
  scoped_refptr<TestDelegate> delegate(new TestDelegate);

  {
    FilePathWatcher watcher;
    SetupWatch(test_file(), &watcher, delegate.get());

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  EXPECT_EQ(0, WaitForEvents(delegate.get()));
}

namespace {
// Used by the DeleteDuringNotify test below.
// Deletes the FilePathWatcher when it's notified.
class Deleter : public FilePathWatcher::Delegate {
 public:
  Deleter(FilePathWatcher* watcher, MessageLoop* loop)
      : watcher_(watcher),
        loop_(loop) {
  }

  virtual void OnFilePathChanged(const FilePath& path) {
    watcher_.reset(NULL);
    loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  scoped_ptr<FilePathWatcher> watcher_;
  MessageLoop* loop_;
};
}  // anonymous namespace

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FilePathWatcherTest, DeleteDuringNotify) {
  FilePathWatcher* watcher = new FilePathWatcher;
  // Takes ownership of watcher.
  scoped_refptr<Deleter> deleter(new Deleter(watcher, &loop_));
  SetupWatch(test_file(), watcher, deleter.get());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  loop_.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter->watcher_.get() == NULL);
}

// Verify that deleting the watcher works even if there is a pending
// notification.
TEST_F(FilePathWatcherTest, DestroyWithPendingNotification) {
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  FilePathWatcher* watcher = new FilePathWatcher;
  SetupWatch(test_file(), watcher, delegate.get());
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ChromeThread::DeleteSoon(ChromeThread::FILE, FROM_HERE, watcher);
  // Success if there is no crash or DCHECK when running the callback.
  WaitForEvents(delegate.get());
}

TEST_F(FilePathWatcherTest, MAYBE(MultipleWatchersSingleFile)) {
  FilePathWatcher watcher1, watcher2;
  scoped_refptr<TestDelegate> delegate1(new TestDelegate);
  scoped_refptr<TestDelegate> delegate2(new TestDelegate);
  SetupWatch(test_file(), &watcher1, delegate1.get());
  SetupWatch(test_file(), &watcher2, delegate2.get());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  EXPECT_LE(1, WaitForEvents(delegate1.get()));
  EXPECT_LE(1, WaitForEvents(delegate2.get()));
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
TEST_F(FilePathWatcherTest, NonExistentDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(file, &watcher, delegate.get());

  ASSERT_TRUE(file_util::CreateDirectory(dir));
  EXPECT_EQ(0, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file, "content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file, "content v2"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(file_util::Delete(file, false));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

// Exercises watch reconfiguration for the case that directories on the path
// are rapidly created.
TEST_F(FilePathWatcherTest, DirectoryChain) {
  FilePath path(temp_dir_->path());
  std::vector<std::string> dir_names;
  for (int i = 0; i < 20; i++) {
    std::string dir(StringPrintf("d%d", i));
    dir_names.push_back(dir);
    path = path.AppendASCII(dir);
  }

  FilePathWatcher watcher;
  FilePath file(path.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(file, &watcher, delegate.get());

  FilePath sub_path(temp_dir_->path());
  for (std::vector<std::string>::const_iterator d(dir_names.begin());
       d != dir_names.end(); ++d) {
    sub_path = sub_path.AppendASCII(*d);
    ASSERT_TRUE(file_util::CreateDirectory(sub_path));
  }
  ASSERT_TRUE(WriteFile(file, "content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file, "content v2"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

TEST_F(FilePathWatcherTest, DisappearingDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(file_util::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(file, &watcher, delegate.get());

  ASSERT_TRUE(file_util::Delete(dir, true));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(file_util::CreateDirectory(dir));
  EXPECT_EQ(0, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file, "content v2"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

// Tests that a file that is deleted and reappears is tracked correctly.
TEST_F(FilePathWatcherTest, DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(test_file(), &watcher, delegate.get());

  ASSERT_TRUE(file_util::Delete(test_file(), false));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

TEST_F(FilePathWatcherTest, WatchDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath file1(dir.AppendASCII("file1"));
  FilePath file2(dir.AppendASCII("file2"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate);
  SetupWatch(dir, &watcher, delegate.get());

  ASSERT_TRUE(file_util::CreateDirectory(dir));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file1, "content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file1, "content v2"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(file_util::Delete(file1, false));
  SyncIfPOSIX();
  EXPECT_LE(1, WaitForEvents(delegate.get()));

  ASSERT_TRUE(WriteFile(file2, "content"));
  EXPECT_LE(1, WaitForEvents(delegate.get()));
}

TEST_F(FilePathWatcherTest, MoveParent) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath dest(temp_dir_->path().AppendASCII("dest"));
  FilePath subdir(dir.AppendASCII("subdir"));
  FilePath file(subdir.AppendASCII("file"));
  scoped_refptr<TestDelegate> file_delegate(new TestDelegate);
  SetupWatch(file, &file_watcher, file_delegate.get());
  scoped_refptr<TestDelegate> subdir_delegate(new TestDelegate);
  SetupWatch(subdir, &subdir_watcher, subdir_delegate.get());

  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(subdir));
  ASSERT_TRUE(WriteFile(file, "content"));
  EXPECT_LE(1, WaitForEvents(file_delegate.get()));
  EXPECT_LE(1, WaitForEvents(subdir_delegate.get()));

  // Move the parent directory.
  file_util::Move(dir, dest);
  EXPECT_LE(1, WaitForEvents(file_delegate.get()));
  EXPECT_LE(1, WaitForEvents(subdir_delegate.get()));

  // Recreate the hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(subdir));
  ASSERT_TRUE(WriteFile(file, "content"));
  EXPECT_LE(1, WaitForEvents(file_delegate.get()));
  EXPECT_LE(1, WaitForEvents(subdir_delegate.get()));
}

TEST_F(FilePathWatcherTest, MoveChild) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath source_dir(temp_dir_->path().AppendASCII("source"));
  FilePath source_subdir(source_dir.AppendASCII("subdir"));
  FilePath source_file(source_subdir.AppendASCII("file"));
  FilePath dest_dir(temp_dir_->path().AppendASCII("dest"));
  FilePath dest_subdir(dest_dir.AppendASCII("subdir"));
  FilePath dest_file(dest_subdir.AppendASCII("file"));
  scoped_refptr<TestDelegate> file_delegate(new TestDelegate);
  SetupWatch(dest_file, &file_watcher, file_delegate.get());
  scoped_refptr<TestDelegate> subdir_delegate(new TestDelegate);
  SetupWatch(dest_subdir, &subdir_watcher, subdir_delegate.get());

  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));
  EXPECT_EQ(0, WaitForEvents(file_delegate.get()));
  EXPECT_EQ(0, WaitForEvents(subdir_delegate.get()));

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(file_util::Move(source_dir, dest_dir));
  EXPECT_LE(1, WaitForEvents(file_delegate.get()));
  EXPECT_LE(1, WaitForEvents(subdir_delegate.get()));
}

}  // namespace
