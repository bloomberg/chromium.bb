// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/file_path_watcher_wrapper.h"

#include "base/bind.h"
#include "base/files/file_path_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class FilePathWatcherWrapperTest : public testing::Test {
 protected:
  class MockFilePathWatcher : public base::files::FilePathWatcher {
   public:
    virtual bool Watch(const FilePath& path, Delegate* delegate) OVERRIDE {
      DCHECK(!delegate_);
      DCHECK(delegate);
      path_ = path;
      delegate_ = delegate;
      return true;
    }

    void OnFilePathChanged() {
      delegate_->OnFilePathChanged(path_);
    }

    void OnFilePathError() {
      delegate_->OnFilePathError(path_);
    }

   private:
    FilePath path_;
    scoped_refptr<Delegate> delegate_;
  };

  class FailingFilePathWatcher : public MockFilePathWatcher {
   public:
    virtual bool Watch(const FilePath& path, Delegate* delegate) OVERRIDE {
      MockFilePathWatcher::Watch(path, delegate);
      return false;
    }
  };

  // Class under test.
  class TestFilePathWatcherWrapper : public FilePathWatcherWrapper {
   public:
    TestFilePathWatcherWrapper() : fail_on_watch_(false) {}

    MockFilePathWatcher* file_path_watcher() {
      return test_watcher_;
    }

    void set_fail_on_watch(bool value) {
      fail_on_watch_ = value;
    }

   private:
    virtual scoped_ptr<base::files::FilePathWatcher>
        CreateFilePathWatcher() OVERRIDE {
      test_watcher_ = fail_on_watch_ ? new FailingFilePathWatcher()
                                     : new MockFilePathWatcher();
      return scoped_ptr<base::files::FilePathWatcher>(test_watcher_);
    }

    MockFilePathWatcher* test_watcher_;
    bool fail_on_watch_;
  };

  virtual void SetUp() OVERRIDE {
    watcher_wrapper_.reset(new TestFilePathWatcherWrapper());
    got_change_ = false;
  }

  bool Watch() {
    return watcher_wrapper_->Watch(
        FilePath(FILE_PATH_LITERAL("some_file_name")),
        base::Bind(&FilePathWatcherWrapperTest::OnChange,
                   base::Unretained(this)));
  }

  void OnChange(bool succeeded) {
    got_change_ = true;
    last_result_ = succeeded;
  }

  scoped_ptr<TestFilePathWatcherWrapper> watcher_wrapper_;
  bool got_change_;
  bool last_result_;
};

TEST_F(FilePathWatcherWrapperTest, Basic) {
  watcher_wrapper_->set_fail_on_watch(true);
  EXPECT_FALSE(Watch());
  EXPECT_FALSE(watcher_wrapper_->IsWatching());
  EXPECT_FALSE(got_change_);

  watcher_wrapper_->set_fail_on_watch(false);
  EXPECT_TRUE(Watch());
  EXPECT_TRUE(watcher_wrapper_->IsWatching());
  EXPECT_FALSE(got_change_);

  EXPECT_TRUE(Watch());
  EXPECT_TRUE(watcher_wrapper_->IsWatching());
  EXPECT_FALSE(got_change_);

  watcher_wrapper_->file_path_watcher()->OnFilePathChanged();
  EXPECT_TRUE(watcher_wrapper_->IsWatching());
  EXPECT_TRUE(got_change_);
  EXPECT_TRUE(last_result_);

  got_change_ = false;
  watcher_wrapper_->file_path_watcher()->OnFilePathError();
  EXPECT_FALSE(watcher_wrapper_->IsWatching());
  EXPECT_TRUE(got_change_);
  EXPECT_FALSE(last_result_);

  got_change_ = false;
  EXPECT_TRUE(Watch());
  EXPECT_TRUE(watcher_wrapper_->IsWatching());
  EXPECT_FALSE(got_change_);
}

TEST_F(FilePathWatcherWrapperTest, Cancel) {
  EXPECT_TRUE(Watch());
  EXPECT_TRUE(watcher_wrapper_->IsWatching());

  watcher_wrapper_->Cancel();
  EXPECT_FALSE(watcher_wrapper_->IsWatching());
  EXPECT_FALSE(got_change_);

  EXPECT_TRUE(Watch());
  EXPECT_TRUE(watcher_wrapper_->IsWatching());
}

}  // namespace

}  // namespace net

