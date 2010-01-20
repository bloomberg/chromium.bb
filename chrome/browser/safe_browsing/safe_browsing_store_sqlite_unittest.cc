// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_sqlite.h"

#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"
#include "chrome/test/file_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const FilePath::CharType kFolderPrefix[] =
    FILE_PATH_LITERAL("SafeBrowsingTestStoreSqlite");

class SafeBrowsingStoreSqliteTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    FilePath temp_dir;
    ASSERT_TRUE(file_util::CreateNewTempDirectory(kFolderPrefix, &temp_dir));

    file_deleter_.reset(new FileAutoDeleter(temp_dir));

    filename_ = temp_dir;
    filename_.AppendASCII("SafeBrowsingTestStore");
    file_util::Delete(filename_, false);

    const FilePath journal_file =
        SafeBrowsingStoreSqlite::JournalFileForFilename(filename_);
    file_util::Delete(journal_file, false);

    store_.reset(new SafeBrowsingStoreSqlite());
    store_->Init(filename_, NULL);
  }
  virtual void TearDown() {
    store_->Delete();
    store_.reset();
    file_deleter_.reset();

    PlatformTest::TearDown();
  }

  scoped_ptr<FileAutoDeleter> file_deleter_;
  FilePath filename_;
  scoped_ptr<SafeBrowsingStoreSqlite> store_;
};

TEST_STORE(SafeBrowsingStoreSqliteTest, store_.get(), filename_);

}  // namespace
