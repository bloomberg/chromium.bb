// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_path_watcher/file_path_watcher.h"

#include <set>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/stl_util-inl.h"
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

class TestDelegate;

// Aggregates notifications from the test delegates and breaks the message loop
// the test thread is waiting on once they all came in.
class NotificationCollector
    : public base::RefCountedThreadSafe<NotificationCollector> {
 public:
  NotificationCollector()
      : loop_(base::MessageLoopProxy::CreateForCurrentThread()) {}

  // Called from the file thread by the delegates.
  void OnChange(TestDelegate* delegate) {
    loop_->PostTask(FROM_HERE,
                    NewRunnableMethod(this,
                                      &NotificationCollector::RecordChange,
                                      make_scoped_refptr(delegate)));
  }

  void Register(TestDelegate* delegate) {
    delegates_.insert(delegate);
  }

  void Reset() {
    signaled_.clear();
  }

 private:
  void RecordChange(TestDelegate* delegate) {
    ASSERT_TRUE(loop_->BelongsToCurrentThread());
    ASSERT_TRUE(delegates_.count(delegate));
    signaled_.insert(delegate);

    // Check whether all delegates have been signaled.
    if (signaled_ == delegates_)
      loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // Set of registered delegates.
  std::set<TestDelegate*> delegates_;

  // Set of signaled delegates.
  std::set<TestDelegate*> signaled_;

  // The loop we should break after all delegates signaled.
  scoped_refptr<base::MessageLoopProxy> loop_;
};

// A mock FilePathWatcher::Delegate for testing. I'd rather use gmock, but it's
// not thread safe for setting expectations, so the test code couldn't safely
// reset expectations while the file watcher is running. In order to allow this,
// we keep simple thread safe status flags in TestDelegate.
class TestDelegate : public FilePathWatcher::Delegate {
 public:
  // The message loop specified by |loop| will be quit if a notification is
  // received while the delegate is |armed_|. Note that the testing code must
  // guarantee |loop| outlives the file thread on which OnFilePathChanged runs.
  explicit TestDelegate(NotificationCollector* collector)
      : collector_(collector) {
    collector_->Register(this);
  }

  virtual void OnFilePathChanged(const FilePath&) {
    collector_->OnChange(this);
  }

 private:
  scoped_refptr<NotificationCollector> collector_;

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
        ui_thread_(BrowserThread::UI, &loop_) {
  }

 protected:
  virtual void SetUp() {
    // Create a separate file thread in order to test proper thread usage.
    file_thread_.reset(new BrowserThread(BrowserThread::FILE));
    file_thread_->Start();
    temp_dir_.reset(new ScopedTempDir);
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    collector_ = new NotificationCollector();
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
    return write_size == static_cast<int>(content.length());
  }

  void SetupWatch(const FilePath& target,
                  FilePathWatcher* watcher,
                  FilePathWatcher::Delegate* delegate) {
    base::WaitableEvent completion(false, false);
    bool result;
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
         new SetupWatchTask(target, watcher, delegate, &result, &completion));
    completion.Wait();
    ASSERT_TRUE(result);
  }

  void WaitForEvents() {
    collector_->Reset();
    loop_.Run();
  }

  NotificationCollector* collector() { return collector_.get(); }

  MessageLoop loop_;
  BrowserThread ui_thread_;
  scoped_ptr<BrowserThread> file_thread_;
  scoped_ptr<ScopedTempDir> temp_dir_;
  scoped_refptr<NotificationCollector> collector_;
};

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, MAYBE(NewFile)) {
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(test_file(), &watcher, delegate.get());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  WaitForEvents();
}

// Verify that modifying the file is caught.
TEST_F(FilePathWatcherTest, MAYBE(ModifiedFile)) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(test_file(), &watcher, delegate.get());

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  WaitForEvents();
}

// Verify that moving the file into place is caught.
TEST_F(FilePathWatcherTest, MAYBE(MovedFile)) {
  FilePath source_file(temp_dir_->path().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(test_file(), &watcher, delegate.get());

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(file_util::Move(source_file, test_file()));
  WaitForEvents();
}

TEST_F(FilePathWatcherTest, MAYBE(DeletedFile)) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(test_file(), &watcher, delegate.get());

  // Now make sure we get notified if the file is deleted.
  file_util::Delete(test_file(), false);
  WaitForEvents();
}

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

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FilePathWatcherTest, DeleteDuringNotify) {
  FilePathWatcher* watcher = new FilePathWatcher;
  // Takes ownership of watcher.
  scoped_refptr<Deleter> deleter(new Deleter(watcher, &loop_));
  SetupWatch(test_file(), watcher, deleter.get());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  WaitForEvents();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter->watcher_.get() == NULL);
}

// Verify that deleting the watcher works even if there is a pending
// notification.
TEST_F(FilePathWatcherTest, DestroyWithPendingNotification) {
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  FilePathWatcher* watcher = new FilePathWatcher;
  SetupWatch(test_file(), watcher, delegate.get());
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, watcher);
}

TEST_F(FilePathWatcherTest, MAYBE(MultipleWatchersSingleFile)) {
  FilePathWatcher watcher1, watcher2;
  scoped_refptr<TestDelegate> delegate1(new TestDelegate(collector()));
  scoped_refptr<TestDelegate> delegate2(new TestDelegate(collector()));
  SetupWatch(test_file(), &watcher1, delegate1.get());
  SetupWatch(test_file(), &watcher2, delegate2.get());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  WaitForEvents();
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
TEST_F(FilePathWatcherTest, NonExistentDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(file, &watcher, delegate.get());

  ASSERT_TRUE(file_util::CreateDirectory(dir));

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  WaitForEvents();

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  WaitForEvents();

  ASSERT_TRUE(file_util::Delete(file, false));
  VLOG(1) << "Waiting for file deletion";
  WaitForEvents();
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
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(file, &watcher, delegate.get());

  FilePath sub_path(temp_dir_->path());
  for (std::vector<std::string>::const_iterator d(dir_names.begin());
       d != dir_names.end(); ++d) {
    sub_path = sub_path.AppendASCII(*d);
    ASSERT_TRUE(file_util::CreateDirectory(sub_path));
  }
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  WaitForEvents();

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file modification";
  WaitForEvents();
}

TEST_F(FilePathWatcherTest, DisappearingDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(file_util::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(file, &watcher, delegate.get());

  ASSERT_TRUE(file_util::Delete(dir, true));
  WaitForEvents();
}

// Tests that a file that is deleted and reappears is tracked correctly.
TEST_F(FilePathWatcherTest, DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(test_file(), &watcher, delegate.get());

  ASSERT_TRUE(file_util::Delete(test_file(), false));
  VLOG(1) << "Waiting for file deletion";
  WaitForEvents();

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  VLOG(1) << "Waiting for file creation";
  WaitForEvents();
}

TEST_F(FilePathWatcherTest, WatchDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath file1(dir.AppendASCII("file1"));
  FilePath file2(dir.AppendASCII("file2"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  SetupWatch(dir, &watcher, delegate.get());

  ASSERT_TRUE(file_util::CreateDirectory(dir));
  VLOG(1) << "Waiting for directory creation";
  WaitForEvents();

  ASSERT_TRUE(WriteFile(file1, "content"));
  VLOG(1) << "Waiting for file1 creation";
  WaitForEvents();

  ASSERT_TRUE(WriteFile(file1, "content v2"));
  VLOG(1) << "Waiting for file1 modification";
  WaitForEvents();

  ASSERT_TRUE(file_util::Delete(file1, false));
  VLOG(1) << "Waiting for file1 deletion";
  WaitForEvents();

  ASSERT_TRUE(WriteFile(file2, "content"));
  VLOG(1) << "Waiting for file2 creation";
  WaitForEvents();
}

TEST_F(FilePathWatcherTest, MoveParent) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath dir(temp_dir_->path().AppendASCII("dir"));
  FilePath dest(temp_dir_->path().AppendASCII("dest"));
  FilePath subdir(dir.AppendASCII("subdir"));
  FilePath file(subdir.AppendASCII("file"));
  scoped_refptr<TestDelegate> file_delegate(new TestDelegate(collector()));
  SetupWatch(file, &file_watcher, file_delegate.get());
  scoped_refptr<TestDelegate> subdir_delegate(new TestDelegate(collector()));
  SetupWatch(subdir, &subdir_watcher, subdir_delegate.get());

  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(subdir));
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  WaitForEvents();

  // Move the parent directory.
  file_util::Move(dir, dest);
  VLOG(1) << "Waiting for directory move";
  WaitForEvents();
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

  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  scoped_refptr<TestDelegate> file_delegate(new TestDelegate(collector()));
  SetupWatch(dest_file, &file_watcher, file_delegate.get());
  scoped_refptr<TestDelegate> subdir_delegate(new TestDelegate(collector()));
  SetupWatch(dest_subdir, &subdir_watcher, subdir_delegate.get());

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(file_util::Move(source_dir, dest_dir));
  WaitForEvents();
}

}  // namespace
