// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/zip_file_creator.h"

#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip_reader.h"

namespace file_manager {

namespace {

void TestCallback(bool* out_success, const base::Closure& quit, bool success) {
  *out_success = success;
  quit.Run();
}

class ZipFileCreatorTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(zip_base_dir()));
  }

  base::FilePath zip_archive_path() const {
    return dir_.path().AppendASCII("test.zip");
  }

  base::FilePath zip_base_dir() const {
    return dir_.path().AppendASCII("files");
  }

 protected:
  base::ScopedTempDir dir_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, FailZipForAbsentFile) {
  base::RunLoop run_loop;
  bool success = true;

  std::vector<base::FilePath> paths;
  paths.push_back(base::FilePath(FILE_PATH_LITERAL("not.exist")));
  (new ZipFileCreator(
       base::Bind(
           &TestCallback, &success, content::GetQuitTaskForRunLoop(&run_loop)),
       zip_base_dir(),
       paths,
       zip_archive_path()))->Start();

  content::RunThisRunLoop(&run_loop);
  EXPECT_FALSE(success);
}

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, SomeFilesZip) {
  // Prepare files.
  const base::FilePath kDir1(FILE_PATH_LITERAL("foo"));
  const base::FilePath kFile1(kDir1.AppendASCII("bar"));
  const base::FilePath kFile2(FILE_PATH_LITERAL("random"));
  const int kRandomDataSize = 100000;
  const std::string kRandomData = base::RandBytesAsString(kRandomDataSize);
  base::CreateDirectory(zip_base_dir().Append(kDir1));
  base::WriteFile(zip_base_dir().Append(kFile1), "123", 3);
  base::WriteFile(zip_base_dir().Append(kFile2),
                  kRandomData.c_str(), kRandomData.size());

  bool success = false;
  base::RunLoop run_loop;

  std::vector<base::FilePath> paths;
  paths.push_back(kDir1);
  paths.push_back(kFile1);
  paths.push_back(kFile2);
  (new ZipFileCreator(
       base::Bind(
           &TestCallback, &success, content::GetQuitTaskForRunLoop(&run_loop)),
       zip_base_dir(),
       paths,
       zip_archive_path()))->Start();

  content::RunThisRunLoop(&run_loop);
  EXPECT_TRUE(success);

  // Check the archive content.
  zip::ZipReader reader;
  ASSERT_TRUE(reader.Open(zip_archive_path()));
  EXPECT_EQ(3, reader.num_entries());
  while (reader.HasMore()) {
    ASSERT_TRUE(reader.OpenCurrentEntryInZip());
    const zip::ZipReader::EntryInfo* entry = reader.current_entry_info();
    // ZipReader returns directory path with trailing slash.
    if (entry->file_path() == kDir1.AsEndingWithSeparator()) {
      EXPECT_TRUE(entry->is_directory());
    } else if (entry->file_path() == kFile1) {
      EXPECT_FALSE(entry->is_directory());
      EXPECT_EQ(3, entry->original_size());
    } else if (entry->file_path() == kFile2) {
      EXPECT_FALSE(entry->is_directory());
      EXPECT_EQ(kRandomDataSize, entry->original_size());

      const base::FilePath out = dir_.path().AppendASCII("archived_content");
      EXPECT_TRUE(reader.ExtractCurrentEntryToFilePath(out));
      EXPECT_TRUE(base::ContentsEqual(zip_base_dir().Append(kFile2), out));
    } else {
      ADD_FAILURE();
    }
    ASSERT_TRUE(reader.AdvanceToNextEntry());
  }
}

}  // namespace file_manager
