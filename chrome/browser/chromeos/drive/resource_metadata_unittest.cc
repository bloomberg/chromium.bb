// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {
namespace {

const char kTestRootResourceId[] = "test_root";

// The changestamp of the resource metadata used in
// ResourceMetadataTest.
const int64 kTestChangestamp = 100;

// Returns the sorted base names from |entries|.
std::vector<std::string> GetSortedBaseNames(
    const ResourceEntryVector& entries) {
  std::vector<std::string> base_names;
  for (size_t i = 0; i < entries.size(); ++i)
    base_names.push_back(entries[i].base_name());
  std::sort(base_names.begin(), base_names.end());

  return base_names;
}

// Increments |count| if |entry| is a file.
void CountFile(int* count, const ResourceEntry& entry) {
  if (!entry.file_info().is_directory())
    ++*count;
}

}  // namespace

class ResourceMetadataTest : public testing::Test {
 protected:
  ResourceMetadataTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  // Creates the following files/directories
  // drive/root/dir1/
  // drive/root/dir2/
  // drive/root/dir1/dir3/
  // drive/root/dir1/file4
  // drive/root/dir1/file5
  // drive/root/dir2/file6
  // drive/root/dir2/file7
  // drive/root/dir2/file8
  // drive/root/dir1/dir3/file9
  // drive/root/dir1/dir3/file10
  static void Init(ResourceMetadata* resource_metadata);

  // Creates a ResourceEntry.
  static ResourceEntry CreateResourceEntry(
      int sequence_id,
      bool is_directory,
      const std::string& parent_resource_id);

  // Adds a ResourceEntry to the metadata tree. Returns true on success.
  static bool AddResourceEntry(ResourceMetadata* resource_metadata,
                                 int sequence_id,
                                 bool is_directory,
                                 const std::string& parent_resource_id);

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::ThreadRestrictions::SetIOAllowed(false);  // For strict thread check.
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());
    resource_metadata_.reset(new ResourceMetadata(temp_dir_.path(),
                                                  blocking_task_runner_));
    Init(resource_metadata_.get());
  }

  virtual void TearDown() OVERRIDE {
    resource_metadata_.reset();
    base::ThreadRestrictions::SetIOAllowed(true);
  }

  // Gets the entry info by path synchronously. Returns NULL on failure.
  scoped_ptr<ResourceEntry> GetEntryInfoByPathSync(
      const base::FilePath& file_path) {
    FileError error = FILE_ERROR_OK;
    scoped_ptr<ResourceEntry> entry;
    resource_metadata_->GetEntryInfoByPath(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_TRUE(error == FILE_ERROR_OK || !entry);
    return entry.Pass();
  }

  // Reads the directory contents by path synchronously. Returns NULL on
  // failure.
  scoped_ptr<ResourceEntryVector> ReadDirectoryByPathSync(
      const base::FilePath& directory_path) {
    FileError error = FILE_ERROR_OK;
    scoped_ptr<ResourceEntryVector> entries;
    resource_metadata_->ReadDirectoryByPath(
        directory_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entries));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_TRUE(error == FILE_ERROR_OK || !entries);
    return entries.Pass();
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

// static
void ResourceMetadataTest::Init(ResourceMetadata* resource_metadata) {
  FileError error = FILE_ERROR_FAILED;
  resource_metadata->Initialize(
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(FILE_ERROR_OK, error);

  // Create mydrive root directory.
  {
    error = FILE_ERROR_FAILED;
    base::FilePath drive_path;
    resource_metadata->AddEntry(
        util::CreateMyDriveRootEntry(kTestRootResourceId),
        google_apis::test_util::CreateCopyResultCallback(&error, &drive_path));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);
  }

  int sequence_id = 1;
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, true, kTestRootResourceId));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, true, kTestRootResourceId));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, true, "resource_id:dir1"));

  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir1"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir1"));

  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir2"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir2"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir2"));

  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir3"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata, sequence_id++, false, "resource_id:dir3"));

  resource_metadata->SetLargestChangestamp(
      kTestChangestamp,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
}

// static
ResourceEntry ResourceMetadataTest::CreateResourceEntry(
    int sequence_id,
    bool is_directory,
    const std::string& parent_resource_id) {
  ResourceEntry entry;
  const std::string sequence_id_str = base::IntToString(sequence_id);
  const std::string title = (is_directory ? "dir" : "file") + sequence_id_str;
  const std::string resource_id = "resource_id:" + title;
  entry.set_title(title);
  entry.set_resource_id(resource_id);
  entry.set_parent_resource_id(parent_resource_id);

  PlatformFileInfoProto* file_info = entry.mutable_file_info();
  file_info->set_is_directory(is_directory);

  if (!is_directory) {
    DriveFileSpecificInfo* file_specific_info =
        entry.mutable_file_specific_info();
    file_info->set_size(sequence_id * 1024);
    file_specific_info->set_file_md5(std::string("md5:") + title);
  } else {
    DriveDirectorySpecificInfo* directory_specific_info =
        entry.mutable_directory_specific_info();
    directory_specific_info->set_changestamp(kTestChangestamp);
  }
  return entry;
}

bool ResourceMetadataTest::AddResourceEntry(
    ResourceMetadata* resource_metadata,
    int sequence_id,
    bool is_directory,
    const std::string& parent_resource_id) {
  ResourceEntry entry = CreateResourceEntry(sequence_id,
                                                      is_directory,
                                                      parent_resource_id);
  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_path;

  resource_metadata->AddEntry(
      entry,
      google_apis::test_util::CreateCopyResultCallback(&error, &drive_path));
  google_apis::test_util::RunBlockingPoolTask();
  return FILE_ERROR_OK == error;
}

TEST_F(ResourceMetadataTest, LargestChangestamp) {
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata(new ResourceMetadata(temp_dir_.path(),
                                             blocking_task_runner_));
  FileError error = FILE_ERROR_FAILED;
  resource_metadata->Initialize(
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(FILE_ERROR_OK, error);

  int64 in_changestamp = 123456;
  resource_metadata->SetLargestChangestamp(
      in_changestamp,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  int64 out_changestamp = 0;
  resource_metadata->GetLargestChangestamp(
      google_apis::test_util::CreateCopyResultCallback(&out_changestamp));
  google_apis::test_util::RunBlockingPoolTask();
  DCHECK_EQ(in_changestamp, out_changestamp);
}

TEST_F(ResourceMetadataTest, GetEntryInfoByResourceId_RootDirectory) {
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata(new ResourceMetadata(temp_dir_.path(),
                                             blocking_task_runner_));
  FileError error = FILE_ERROR_FAILED;
  resource_metadata->Initialize(
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(FILE_ERROR_OK, error);

  base::FilePath drive_file_path;
  scoped_ptr<ResourceEntry> entry;

  // Look up the root directory by its resource ID.
  resource_metadata->GetEntryInfoByResourceId(
      util::kDriveGrandRootSpecialResourceId,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive"), drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("drive", entry->base_name());
}

TEST_F(ResourceMetadataTest, GetEntryInfoByResourceId) {
  // Confirm that an existing file is found.
  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<ResourceEntry> entry;
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file4",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
            drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file4", entry->base_name());

  // Confirm that a non existing file is not found.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetEntryInfoByResourceId(
      "file:non_existing",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());
}

TEST_F(ResourceMetadataTest, GetEntryInfoByPath) {
  // Confirm that an existing file is found.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<ResourceEntry> entry;
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file4", entry->base_name());

  // Confirm that a non existing file is not found.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existing"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Confirm that the root is found.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(entry.get());

  // Confirm that a non existing file is not found at the root level.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("non_existing"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Confirm that an entry is not found with a wrong root.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("non_existing/root"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());
}

TEST_F(ResourceMetadataTest, ReadDirectoryByPath) {
  // Confirm that an existing directory is found.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<ResourceEntryVector> entries;
  resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());
  // The order is not guaranteed so we should sort the base names.
  std::vector<std::string> base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Confirm that a non existing directory is not found.
  error = FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/non_existing"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entries.get());

  // Confirm that reading a file results in FILE_ERROR_NOT_A_DIRECTORY.
  error = FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_FALSE(entries.get());
}

TEST_F(ResourceMetadataTest, GetEntryInfoPairByPaths) {
  // Confirm that existing two files are found.
  scoped_ptr<EntryInfoPairResult> pair_result;
  resource_metadata_->GetEntryInfoPairByPaths(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file5"),
      google_apis::test_util::CreateCopyResultCallback(&pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should be found.
  EXPECT_EQ(FILE_ERROR_OK, pair_result->first.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
            pair_result->first.path);
  ASSERT_TRUE(pair_result->first.entry.get());
  EXPECT_EQ("file4", pair_result->first.entry->base_name());
  // The second entry should be found.
  EXPECT_EQ(FILE_ERROR_OK, pair_result->second.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/file5"),
            pair_result->second.path);
  ASSERT_TRUE(pair_result->second.entry.get());
  EXPECT_EQ("file5", pair_result->second.entry->base_name());

  // Confirm that the first non existent file is not found.
  pair_result.reset();
  resource_metadata_->GetEntryInfoPairByPaths(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existent"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file5"),
      google_apis::test_util::CreateCopyResultCallback(&pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should not be found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, pair_result->first.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existent"),
            pair_result->first.path);
  ASSERT_FALSE(pair_result->first.entry.get());
  // The second entry should not be found, because the first one failed.
  EXPECT_EQ(FILE_ERROR_FAILED, pair_result->second.error);
  EXPECT_EQ(base::FilePath(), pair_result->second.path);
  ASSERT_FALSE(pair_result->second.entry.get());

  // Confirm that the second non existent file is not found.
  pair_result.reset();
  resource_metadata_->GetEntryInfoPairByPaths(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existent"),
      google_apis::test_util::CreateCopyResultCallback(&pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should be found.
  EXPECT_EQ(FILE_ERROR_OK, pair_result->first.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
            pair_result->first.path);
  ASSERT_TRUE(pair_result->first.entry.get());
  EXPECT_EQ("file4", pair_result->first.entry->base_name());
  // The second entry should not be found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, pair_result->second.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existent"),
            pair_result->second.path);
  ASSERT_FALSE(pair_result->second.entry.get());
}

TEST_F(ResourceMetadataTest, RemoveEntry) {
  // Make sure file9 is found.
  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  const std::string file9_resource_id = "resource_id:file9";
  scoped_ptr<ResourceEntry> entry;
  resource_metadata_->GetEntryInfoByResourceId(
      file9_resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"),
            drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file9", entry->base_name());

  // Remove file9 using RemoveEntry.
  resource_metadata_->RemoveEntry(
      file9_resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"),
            drive_file_path);

  // file9 should no longer exist.
  resource_metadata_->GetEntryInfoByResourceId(
      file9_resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Look for dir3.
  const std::string dir3_resource_id = "resource_id:dir3";
  resource_metadata_->GetEntryInfoByResourceId(
      dir3_resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"),
            drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("dir3", entry->base_name());

  // Remove dir3 using RemoveEntry.
  resource_metadata_->RemoveEntry(
      dir3_resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1"), drive_file_path);

  // dir3 should no longer exist.
  resource_metadata_->GetEntryInfoByResourceId(
      dir3_resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Remove unknown resource_id using RemoveEntry.
  resource_metadata_->RemoveEntry(
      "foo",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  // Try removing root. This should fail.
  resource_metadata_->RemoveEntry(
      util::kDriveGrandRootSpecialResourceId,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_ACCESS_DENIED, error);
}

TEST_F(ResourceMetadataTest, MoveEntryToDirectory) {
  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<ResourceEntry> entry;

  // Move file8 to drive/dir1.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2/file8"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/file8"),
            drive_file_path);

  // Look up the entry by its resource id and make sure it really moved.
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file8",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/file8"),
            drive_file_path);

  // Move non-existent file to drive/dir1. This should fail.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2/file8"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Move existing file to non-existent directory. This should fail.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file8"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir4"),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Move existing file to existing file (non-directory). This should fail.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file8"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Move the file to root.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file8"),
      base::FilePath::FromUTF8Unsafe("drive/root"),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/file8"),
            drive_file_path);

  // Move the file from root.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root/file8"),
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir2/file8"),
            drive_file_path);

  // Make sure file is still ok.
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file8",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir2/file8"),
            drive_file_path);
}

TEST_F(ResourceMetadataTest, RenameEntry) {
  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<ResourceEntry> entry;

  // Rename file8 to file11.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2/file8"),
      "file11",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir2/file11"),
            drive_file_path);

  // Lookup the file by resource id to make sure the file actually got renamed.
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file8",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir2/file11"),
            drive_file_path);

  // Rename to file7 to force a duplicate name.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2/file11"),
      "file7",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir2/file7 (2)"),
            drive_file_path);

  // Rename to same name. This should fail.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2/file7 (2)"),
      "file7 (2)",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_EXISTS, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Rename non-existent.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2/file11"),
      "file11",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);
}

TEST_F(ResourceMetadataTest, RefreshEntry) {
  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<ResourceEntry> entry;

  // Get file9.
  entry = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file9", entry->base_name());
  ASSERT_TRUE(!entry->file_info().is_directory());
  EXPECT_EQ("md5:file9", entry->file_specific_info().file_md5());

  // Rename it.
  ResourceEntry file_entry(*entry);
  file_entry.set_title("file100");
  entry.reset();
  resource_metadata_->RefreshEntry(
      file_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file100"),
            drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file100", entry->base_name());
  ASSERT_TRUE(!entry->file_info().is_directory());
  EXPECT_EQ("md5:file9", entry->file_specific_info().file_md5());

  // Update the file md5.
  const std::string updated_md5("md5:updated");
  file_entry = *entry;
  file_entry.mutable_file_specific_info()->set_file_md5(updated_md5);
  entry.reset();
  resource_metadata_->RefreshEntry(
      file_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file100"),
            drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file100", entry->base_name());
  ASSERT_TRUE(!entry->file_info().is_directory());
  EXPECT_EQ(updated_md5, entry->file_specific_info().file_md5());

  // Make sure we get the same thing from GetEntryInfoByPath.
  entry = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file100"));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file100", entry->base_name());
  ASSERT_TRUE(!entry->file_info().is_directory());
  EXPECT_EQ(updated_md5, entry->file_specific_info().file_md5());

  // Get dir2.
  entry = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("dir2", entry->base_name());
  ASSERT_TRUE(entry->file_info().is_directory());

  // Change the name to dir100 and change the parent to drive/dir1/dir3.
  ResourceEntry dir_entry(*entry);
  dir_entry.set_title("dir100");
  dir_entry.set_parent_resource_id("resource_id:dir3");
  entry.reset();
  resource_metadata_->RefreshEntry(
      dir_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/dir100"),
            drive_file_path);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("dir100", entry->base_name());
  EXPECT_TRUE(entry->file_info().is_directory());
  EXPECT_EQ("resource_id:dir2", entry->resource_id());

  // Make sure the children have moved over. Test file6.
  entry = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/dir100/file6"));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file6", entry->base_name());

  // Make sure dir2 no longer exists.
  entry = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"));
  EXPECT_FALSE(entry.get());

  // Cannot refresh root.
  dir_entry.Clear();
  dir_entry.set_resource_id(util::kDriveGrandRootSpecialResourceId);
  dir_entry.set_title("new-root-name");
  dir_entry.set_parent_resource_id("resource_id:dir1");
  entry.reset();
  resource_metadata_->RefreshEntry(
      dir_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION, error);
}

TEST_F(ResourceMetadataTest, RefreshDirectory_EmtpyMap) {
  base::FilePath kDirectoryPath(FILE_PATH_LITERAL("drive/root/dir1"));
  const int64 kNewChangestamp = kTestChangestamp + 1;

  // Read the directory.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<ResourceEntryVector> entries;
  entries = ReadDirectoryByPathSync(base::FilePath(kDirectoryPath));
  ASSERT_TRUE(entries.get());
  // "file4", "file5", "dir3" should exist in drive/dir1.
  ASSERT_EQ(3U, entries->size());
  std::vector<std::string> base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Get the directory.
  scoped_ptr<ResourceEntry> dir1_proto;
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The changestamp should be initially kTestChangestamp.
  EXPECT_EQ(kTestChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Update the directory with an empty map.
  base::FilePath file_path;
  ResourceEntryMap entry_map;
  resource_metadata_->RefreshDirectory(
      DirectoryFetchInfo(dir1_proto->resource_id(), kNewChangestamp),
      entry_map,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(kDirectoryPath, file_path);

  // Get the directory again.
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The new changestamp should be set.
  EXPECT_EQ(kNewChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Read the directory again.
  entries = ReadDirectoryByPathSync(base::FilePath(kDirectoryPath));
  ASSERT_TRUE(entries.get());
  // All entries ("file4", "file5", "dir3") should be gone now, as
  // RefreshDirectory() was called with an empty map.
  ASSERT_TRUE(entries->empty());
}

TEST_F(ResourceMetadataTest, RefreshDirectory_NonEmptyMap) {
  base::FilePath kDirectoryPath(FILE_PATH_LITERAL("drive/root/dir1"));
  const int64 kNewChangestamp = kTestChangestamp + 1;

  // Read the directory.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<ResourceEntryVector> entries;
  entries = ReadDirectoryByPathSync(kDirectoryPath);
  ASSERT_TRUE(entries.get());
  // "file4", "file5", "dir3" should exist in drive/dir1.
  ASSERT_EQ(3U, entries->size());
  std::vector<std::string> base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Get the directory dir1.
  scoped_ptr<ResourceEntry> dir1_proto;
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The changestamp should be initially kTestChangestamp.
  EXPECT_EQ(kTestChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Get the directory dir2 (existing non-child directory).
  // This directory will be moved to "drive/dir1/dir2".
  scoped_ptr<ResourceEntry> dir2_proto;
  dir2_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"));
  ASSERT_TRUE(dir2_proto.get());
  EXPECT_EQ(kTestChangestamp,
            dir2_proto->directory_specific_info().changestamp());
  // Change the parent resource ID, as dir2 will be moved to "drive/dir1/dir2".
  dir2_proto->set_parent_resource_id(dir1_proto->resource_id());

  // Get the directory dir3 (existing child directory).
  // This directory will remain as "drive/dir1/dir3".
  scoped_ptr<ResourceEntry> dir3_proto;
  dir3_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"));
  ASSERT_TRUE(dir3_proto.get());
  EXPECT_EQ(kTestChangestamp,
            dir3_proto->directory_specific_info().changestamp());

  // Create a map.
  ResourceEntryMap entry_map;

  // Add a new file to the map.
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  new_file.set_parent_resource_id(dir1_proto->resource_id());
  entry_map["new_file_id"] = new_file;

  // Add a new directory to the map.
  ResourceEntry new_directory;
  new_directory.set_title("new_directory");
  new_directory.set_resource_id("new_directory_id");
  new_directory.set_parent_resource_id(dir1_proto->resource_id());
  new_directory.mutable_file_info()->set_is_directory(true);
  entry_map["new_directory_id"] = new_directory;

  // Add dir2 and dir3 as well.
  entry_map[dir2_proto->resource_id()] = *dir2_proto;
  entry_map[dir3_proto->resource_id()] = *dir3_proto;

  // Update the directory with the map.
  base::FilePath file_path;
  resource_metadata_->RefreshDirectory(
      DirectoryFetchInfo(dir1_proto->resource_id(), kNewChangestamp),
      entry_map,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(kDirectoryPath, file_path);

  // Get the directory again.
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The new changestamp should be set.
  EXPECT_EQ(kNewChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Read the directory again.
  entries = ReadDirectoryByPathSync(kDirectoryPath);
  ASSERT_TRUE(entries.get());
  // "file4", "file5", should be gone now. "dir3" should remain. "new_file"
  // "new_directory", "dir2" should now be added.
  ASSERT_EQ(4U, entries->size());
  base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir2", base_names[0]);
  EXPECT_EQ("dir3", base_names[1]);
  EXPECT_EQ("new_directory", base_names[2]);
  EXPECT_EQ("new_file", base_names[3]);

  // Get the new directory.
  scoped_ptr<ResourceEntry> new_directory_proto;
  new_directory_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/new_directory"));
  ASSERT_TRUE(new_directory_proto.get());
  // The changestamp should be 0 for a new directory.
  EXPECT_EQ(0, new_directory_proto->directory_specific_info().changestamp());

  // Get the directory dir3 (existing child directory) again.
  dir3_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"));
  ASSERT_TRUE(dir3_proto.get());
  // The changestamp should not be changed.
  EXPECT_EQ(kTestChangestamp,
            dir3_proto->directory_specific_info().changestamp());

  // Read the directory dir3. The contents should remain.
  // See the comment at Init() for the contents of the dir3.
  entries = ReadDirectoryByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"));
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(2U, entries->size());

  // Get the directory dir2 (existing non-child directory) again using the
  // old path.  This should fail, as dir2 is now moved to drive/dir1/dir2.
  dir2_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"));
  ASSERT_FALSE(dir2_proto.get());

  // Get the directory dir2 (existing non-child directory) again using the
  // new path. This should succeed.
  dir2_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir2"));
  ASSERT_TRUE(dir2_proto.get());
  // The changestamp should not be changed.
  EXPECT_EQ(kTestChangestamp,
            dir2_proto->directory_specific_info().changestamp());

  // Read the directory dir2. The contents should remain.
  // See the comment at Init() for the contents of the dir2.
  entries = ReadDirectoryByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir2"));
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());
}

TEST_F(ResourceMetadataTest, RefreshDirectory_WrongParentResourceId) {
  base::FilePath kDirectoryPath(FILE_PATH_LITERAL("drive/root/dir1"));
  const int64 kNewChangestamp = kTestChangestamp + 1;

  // Get the directory dir1.
  scoped_ptr<ResourceEntry> dir1_proto;
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());

  // Create a map and add a new file to it.
  ResourceEntryMap entry_map;
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  // Set a random parent resource ID. This entry should not be added because
  // the parent resource ID does not match dir1_proto->resource_id().
  new_file.set_parent_resource_id("some-random-resource-id");
  entry_map["new_file_id"] = new_file;

  // Update the directory with the map.
  base::FilePath file_path;
  FileError error = FILE_ERROR_FAILED;
  resource_metadata_->RefreshDirectory(
      DirectoryFetchInfo(dir1_proto->resource_id(), kNewChangestamp),
      entry_map,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(kDirectoryPath, file_path);

  // Read the directory. Confirm that the new file is not added.
  scoped_ptr<ResourceEntryVector> entries;
  entries = ReadDirectoryByPathSync(kDirectoryPath);
  ASSERT_TRUE(entries.get());
  ASSERT_TRUE(entries->empty());
}

TEST_F(ResourceMetadataTest, AddEntry) {
  int sequence_id = 100;
  ResourceEntry file_entry = CreateResourceEntry(
      sequence_id++, false, "resource_id:dir3");

  FileError error = FILE_ERROR_FAILED;
  base::FilePath drive_file_path;

  // Add to dir3.
  resource_metadata_->AddEntry(
      file_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file100"),
            drive_file_path);

  // Add a directory.
  ResourceEntry dir_entry = CreateResourceEntry(
      sequence_id++, true, "resource_id:dir1");

  resource_metadata_->AddEntry(
      dir_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir101"),
            drive_file_path);

  // Add to an invalid parent.
  ResourceEntry file_entry3 = CreateResourceEntry(
      sequence_id++, false, "resource_id:invalid");

  resource_metadata_->AddEntry(
      file_entry3,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  // Add an existing file.
  resource_metadata_->AddEntry(
      file_entry,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_EXISTS, error);
}

TEST_F(ResourceMetadataTest, GetChildDirectories) {
  std::set<base::FilePath> child_directories;

  // file9: not a directory, so no children.
  resource_metadata_->GetChildDirectories(
      "resource_id:file9",
      google_apis::test_util::CreateCopyResultCallback(&child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(child_directories.empty());

  // dir2: no child directories.
  resource_metadata_->GetChildDirectories(
      "resource_id:dir2",
      google_apis::test_util::CreateCopyResultCallback(&child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(child_directories.empty());

  // dir1: dir3 is the only child
  resource_metadata_->GetChildDirectories(
      "resource_id:dir1",
      google_apis::test_util::CreateCopyResultCallback(&child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(1u, child_directories.size());
  EXPECT_EQ(1u, child_directories.count(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3")));

  // Add a few more directories to make sure deeper nesting works.
  // dir2/dir100
  // dir2/dir101
  // dir2/dir101/dir102
  // dir2/dir101/dir103
  // dir2/dir101/dir104
  // dir2/dir101/dir102/dir105
  // dir2/dir101/dir102/dir105/dir106
  // dir2/dir101/dir102/dir105/dir106/dir107
  int sequence_id = 100;
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir2"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir2"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir102"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir105"));
  ASSERT_TRUE(AddResourceEntry(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir106"));

  resource_metadata_->GetChildDirectories(
      "resource_id:dir2",
      google_apis::test_util::CreateCopyResultCallback(&child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(8u, child_directories.size());
  EXPECT_EQ(1u, child_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101")));
  EXPECT_EQ(1u, child_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101/dir104")));
  EXPECT_EQ(1u, child_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101/dir102/dir105/dir106/dir107")));
}

TEST_F(ResourceMetadataTest, Reset) {
  // The grand root has "root" which is not empty.
  scoped_ptr<ResourceEntryVector> entries;
  entries = ReadDirectoryByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/root"));
  ASSERT_TRUE(entries.get());
  ASSERT_FALSE(entries->empty());

  // Reset.
  FileError error = FILE_ERROR_FAILED;
  resource_metadata_->Reset(
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  base::FilePath drive_file_path;
  scoped_ptr<ResourceEntry> entry;

  // change stamp should be reset.
  int64 changestamp = -1;
  resource_metadata_->GetLargestChangestamp(
      google_apis::test_util::CreateCopyResultCallback(&changestamp));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(0, changestamp);

  // root should continue to exist.
  entry = GetEntryInfoByPathSync(base::FilePath::FromUTF8Unsafe("drive"));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("drive", entry->base_name());
  ASSERT_TRUE(entry->file_info().is_directory());
  EXPECT_EQ(util::kDriveGrandRootSpecialResourceId, entry->resource_id());

  // There is "other", which are both empty.
  entries = ReadDirectoryByPathSync(base::FilePath::FromUTF8Unsafe("drive"));
  ASSERT_TRUE(entries.get());
  EXPECT_EQ(1U, entries->size());

  scoped_ptr<ResourceEntryVector> entries_in_other =
      ReadDirectoryByPathSync(base::FilePath::FromUTF8Unsafe("drive/other"));
  ASSERT_TRUE(entries_in_other.get());
  EXPECT_TRUE(entries_in_other->empty());
}

TEST_F(ResourceMetadataTest, IterateEntries) {
  int count = 0;
  bool completed = false;
  resource_metadata_->IterateEntries(
      base::Bind(&CountFile, &count),
      base::Bind(google_apis::test_util::CreateCopyResultCallback(&completed),
                 true));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(7, count);
  EXPECT_TRUE(completed);
}

}  // namespace internal
}  // namespace drive
