// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"

#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"
#include "chrome/test/file_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const FilePath::CharType kFolderPrefix[] =
    FILE_PATH_LITERAL("SafeBrowsingTestStoreFile");

class SafeBrowsingStoreFileTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    FilePath temp_dir;
    ASSERT_TRUE(file_util::CreateNewTempDirectory(kFolderPrefix, &temp_dir));

    file_deleter_.reset(new FileAutoDeleter(temp_dir));

    filename_ = temp_dir;
    filename_.AppendASCII("SafeBrowsingTestStore");
    file_util::Delete(filename_, false);

    // Make sure an old temporary file isn't hanging around.
    const FilePath temp_file =
        SafeBrowsingStoreFile::TemporaryFileForFilename(filename_);
    file_util::Delete(temp_file, false);

    store_.reset(new SafeBrowsingStoreFile());
    store_->Init(filename_, NULL);
  }
  virtual void TearDown() {
    if (store_.get())
      store_->Delete();
    store_.reset();
    file_deleter_.reset();

    PlatformTest::TearDown();
  }

  scoped_ptr<FileAutoDeleter> file_deleter_;
  FilePath filename_;
  scoped_ptr<SafeBrowsingStoreFile> store_;
};

TEST_STORE(SafeBrowsingStoreFileTest, store_.get(), filename_);

// Test that Delete() deletes the temporary store, if present.
TEST_F(SafeBrowsingStoreFileTest, DeleteTemp) {
    const FilePath temp_file =
        SafeBrowsingStoreFile::TemporaryFileForFilename(filename_);

  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_FALSE(file_util::PathExists(temp_file));

  // Starting a transaction creates a temporary file.
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_TRUE(file_util::PathExists(temp_file));

  // Pull the rug out from under the existing store, simulating a
  // crash.
  store_.reset(new SafeBrowsingStoreFile());
  store_->Init(filename_, NULL);
  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_TRUE(file_util::PathExists(temp_file));

  // Make sure the temporary file is deleted.
  EXPECT_TRUE(store_->Delete());
  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_FALSE(file_util::PathExists(temp_file));
}

// TODO(shess): Test corruption-handling?

}  // namespace
