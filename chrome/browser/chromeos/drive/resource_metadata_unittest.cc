// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
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

// Creates a ResourceEntry for a directory with explicitly set resource_id.
ResourceEntry CreateDirectoryEntryWithResourceId(
    const std::string& title,
    const std::string& resource_id,
    const std::string& parent_local_id) {
  ResourceEntry entry;
  entry.set_title(title);
  entry.set_resource_id(resource_id);
  entry.set_parent_local_id(parent_local_id);
  entry.mutable_file_info()->set_is_directory(true);
  entry.mutable_directory_specific_info()->set_changestamp(kTestChangestamp);
  return entry;
}

// Creates a ResourceEntry for a directory.
ResourceEntry CreateDirectoryEntry(const std::string& title,
                                   const std::string& parent_local_id) {
  return CreateDirectoryEntryWithResourceId(
      title, "id:" + title, parent_local_id);
}

// Creates a ResourceEntry for a file with explicitly set resource_id.
ResourceEntry CreateFileEntryWithResourceId(
    const std::string& title,
    const std::string& resource_id,
    const std::string& parent_local_id) {
  ResourceEntry entry;
  entry.set_title(title);
  entry.set_resource_id(resource_id);
  entry.set_parent_local_id(parent_local_id);
  entry.mutable_file_info()->set_is_directory(false);
  entry.mutable_file_info()->set_size(1024);
  entry.mutable_file_specific_info()->set_md5("md5:" + title);
  return entry;
}

// Creates a ResourceEntry for a file.
ResourceEntry CreateFileEntry(const std::string& title,
                              const std::string& parent_local_id) {
  return CreateFileEntryWithResourceId(title, "id:" + title, parent_local_id);
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
void SetUpEntries(ResourceMetadata* resource_metadata) {
  // Create mydrive root directory.
  std::string local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      util::CreateMyDriveRootEntry(kTestRootResourceId), &local_id));
  const std::string root_local_id = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateDirectoryEntry("dir1", root_local_id), &local_id));
  const std::string local_id_dir1 = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateDirectoryEntry("dir2", root_local_id), &local_id));
  const std::string local_id_dir2 = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateDirectoryEntry("dir3", local_id_dir1), &local_id));
  const std::string local_id_dir3 = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file4", local_id_dir1), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file5", local_id_dir1), &local_id));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file6", local_id_dir2), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file7", local_id_dir2), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file8", local_id_dir2), &local_id));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file9", local_id_dir3), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file10", local_id_dir3), &local_id));

  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata->SetLargestChangestamp(kTestChangestamp));
}

}  // namespace

// Tests for methods invoked from the UI thread.
class ResourceMetadataTestOnUIThread : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::ThreadRestrictions::SetIOAllowed(false);  // For strict thread check.
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), blocking_task_runner_.get()));
    bool success = false;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(),
        FROM_HERE,
        base::Bind(&ResourceMetadataStorage::Initialize,
                   base::Unretained(metadata_storage_.get())),
        google_apis::test_util::CreateCopyResultCallback(&success));
    test_util::RunBlockingPoolTask();
    ASSERT_TRUE(success);

    resource_metadata_.reset(new ResourceMetadata(metadata_storage_.get(),
                                                  blocking_task_runner_));

    FileError error = FILE_ERROR_FAILED;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(),
        FROM_HERE,
        base::Bind(&ResourceMetadata::Initialize,
                   base::Unretained(resource_metadata_.get())),
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);

    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SetUpEntries,
                   base::Unretained(resource_metadata_.get())));
    test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    metadata_storage_.reset();
    resource_metadata_.reset();
    base::ThreadRestrictions::SetIOAllowed(true);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
};

TEST_F(ResourceMetadataTestOnUIThread, GetResourceEntryByPath) {
  // Confirm that an existing file is found.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<ResourceEntry> entry;
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file4", entry->base_name());

  // Confirm that a non existing file is not found.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existing"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Confirm that the root is found.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("drive"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(entry.get());

  // Confirm that a non existing file is not found at the root level.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("non_existing"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Confirm that an entry is not found with a wrong root.
  error = FILE_ERROR_FAILED;
  entry.reset();
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("non_existing/root"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());
}

TEST_F(ResourceMetadataTestOnUIThread, ReadDirectoryByPath) {
  // Confirm that an existing directory is found.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<ResourceEntryVector> entries;
  resource_metadata_->ReadDirectoryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entries));
  test_util::RunBlockingPoolTask();
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
  resource_metadata_->ReadDirectoryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("drive/root/non_existing"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entries));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entries.get());

  // Confirm that reading a file results in FILE_ERROR_NOT_A_DIRECTORY.
  error = FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_->ReadDirectoryByPathOnUIThread(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"),
      google_apis::test_util::CreateCopyResultCallback(&error, &entries));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_FALSE(entries.get());
}

// Tests for methods running on the blocking task runner.
class ResourceMetadataTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    resource_metadata_.reset(new ResourceMetadata(
        metadata_storage_.get(), base::MessageLoopProxy::current()));

    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->Initialize());

    SetUpEntries(resource_metadata_.get());
  }

  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
};

TEST_F(ResourceMetadataTest, LargestChangestamp) {
  const int64 kChangestamp = 123456;
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->SetLargestChangestamp(kChangestamp));
  EXPECT_EQ(kChangestamp, resource_metadata_->GetLargestChangestamp());
}

TEST_F(ResourceMetadataTest, RefreshEntry) {
  base::FilePath drive_file_path;
  ResourceEntry entry;

  // Get file9.
  std::string file_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"), &file_id));
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(file_id, &entry));
  EXPECT_EQ("file9", entry.base_name());
  EXPECT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ("md5:file9", entry.file_specific_info().md5());

  // Rename it.
  ResourceEntry file_entry(entry);
  file_entry.set_title("file100");
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->RefreshEntry(file_entry));

  EXPECT_EQ("drive/root/dir1/dir3/file100",
            resource_metadata_->GetFilePath(file_id).AsUTF8Unsafe());
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(file_id, &entry));
  EXPECT_EQ("file100", entry.base_name());
  EXPECT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ("md5:file9", entry.file_specific_info().md5());

  // Update the file md5.
  const std::string updated_md5("md5:updated");
  file_entry = entry;
  file_entry.mutable_file_specific_info()->set_md5(updated_md5);
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->RefreshEntry(file_entry));

  EXPECT_EQ("drive/root/dir1/dir3/file100",
            resource_metadata_->GetFilePath(file_id).AsUTF8Unsafe());
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(file_id, &entry));
  EXPECT_EQ("file100", entry.base_name());
  EXPECT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ(updated_md5, entry.file_specific_info().md5());

  // Make sure we get the same thing from GetResourceEntryByPath.
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file100"), &entry));
  EXPECT_EQ("file100", entry.base_name());
  ASSERT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ(updated_md5, entry.file_specific_info().md5());

  // Get dir2.
  entry.Clear();
  std::string dir_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"), &dir_id));
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(dir_id, &entry));
  EXPECT_EQ("dir2", entry.base_name());
  ASSERT_TRUE(entry.file_info().is_directory());

  // Get dir3's ID.
  std::string dir3_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"), &dir3_id));

  // Change the name to dir100 and change the parent to drive/dir1/dir3.
  ResourceEntry dir_entry(entry);
  dir_entry.set_title("dir100");
  dir_entry.set_parent_local_id(dir3_id);
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RefreshEntry(dir_entry));

  EXPECT_EQ("drive/root/dir1/dir3/dir100",
            resource_metadata_->GetFilePath(dir_id).AsUTF8Unsafe());
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(dir_id, &entry));
  EXPECT_EQ("dir100", entry.base_name());
  EXPECT_TRUE(entry.file_info().is_directory());
  EXPECT_EQ("id:dir2", entry.resource_id());

  // Make sure the children have moved over. Test file6.
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/dir100/file6"),
      &entry));
  EXPECT_EQ("file6", entry.base_name());

  // Make sure dir2 no longer exists.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"), &entry));

  // Make sure that directory cannot move under a file.
  dir_entry.set_parent_local_id(file_id);
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY,
            resource_metadata_->RefreshEntry(dir_entry));

  // Cannot refresh root.
  dir_entry.Clear();
  dir_entry.set_resource_id(util::kDriveGrandRootLocalId);
  dir_entry.set_local_id(util::kDriveGrandRootLocalId);
  dir_entry.set_title("new-root-name");
  dir_entry.set_parent_local_id(dir3_id);
  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION,
            resource_metadata_->RefreshEntry(dir_entry));
}

TEST_F(ResourceMetadataTest, GetSubDirectoriesRecursively) {
  std::set<base::FilePath> sub_directories;

  // file9: not a directory, so no children.
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"), &local_id));
  resource_metadata_->GetSubDirectoriesRecursively(local_id, &sub_directories);
  EXPECT_TRUE(sub_directories.empty());

  // dir2: no child directories.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"), &local_id));
  resource_metadata_->GetSubDirectoriesRecursively(local_id, &sub_directories);
  EXPECT_TRUE(sub_directories.empty());
  const std::string dir2_id = local_id;

  // dir1: dir3 is the only child
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"), &local_id));
  resource_metadata_->GetSubDirectoriesRecursively(local_id, &sub_directories);
  EXPECT_EQ(1u, sub_directories.size());
  EXPECT_EQ(1u, sub_directories.count(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3")));
  sub_directories.clear();

  // Add a few more directories to make sure deeper nesting works.
  // dir2/dir100
  // dir2/dir101
  // dir2/dir101/dir102
  // dir2/dir101/dir103
  // dir2/dir101/dir104
  // dir2/dir101/dir104/dir105
  // dir2/dir101/dir104/dir105/dir106
  // dir2/dir101/dir104/dir105/dir106/dir107
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir100", dir2_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir101", dir2_id), &local_id));
  const std::string dir101_id = local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir102", dir101_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir103", dir101_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir104", dir101_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir105", local_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir106", local_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir107", local_id), &local_id));

  resource_metadata_->GetSubDirectoriesRecursively(dir2_id, &sub_directories);
  EXPECT_EQ(8u, sub_directories.size());
  EXPECT_EQ(1u, sub_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101")));
  EXPECT_EQ(1u, sub_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101/dir104")));
  EXPECT_EQ(1u, sub_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101/dir104/dir105/dir106/dir107")));
}

TEST_F(ResourceMetadataTest, AddEntry) {
  base::FilePath drive_file_path;

  // Add a file to dir3.
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"), &local_id));
  ResourceEntry file_entry = CreateFileEntry("file100", local_id);
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(file_entry, &local_id));
  EXPECT_EQ("drive/root/dir1/dir3/file100",
            resource_metadata_->GetFilePath(local_id).AsUTF8Unsafe());

  // Add a directory.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"), &local_id));
  ResourceEntry dir_entry = CreateDirectoryEntry("dir101", local_id);
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(dir_entry, &local_id));
  EXPECT_EQ("drive/root/dir1/dir101",
            resource_metadata_->GetFilePath(local_id).AsUTF8Unsafe());

  // Add to an invalid parent.
  ResourceEntry file_entry3 = CreateFileEntry("file103", "id:invalid");
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            resource_metadata_->AddEntry(file_entry3, &local_id));

  // Add an existing file.
  EXPECT_EQ(FILE_ERROR_EXISTS,
            resource_metadata_->AddEntry(file_entry, &local_id));
}

TEST_F(ResourceMetadataTest, RemoveEntry) {
  // Make sure file9 is found.
  std::string file9_local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"),
      &file9_local_id));
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file9_local_id, &entry));
  EXPECT_EQ("file9", entry.base_name());

  // Remove file9.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RemoveEntry(file9_local_id));

  // file9 should no longer exist.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryById(
      file9_local_id, &entry));

  // Look for dir3.
  std::string dir3_local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"), &dir3_local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir3_local_id, &entry));
  EXPECT_EQ("dir3", entry.base_name());

  // Remove dir3.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RemoveEntry(dir3_local_id));

  // dir3 should no longer exist.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryById(
      dir3_local_id, &entry));

  // Remove unknown local_id using RemoveEntry.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->RemoveEntry("foo"));

  // Try removing root. This should fail.
  EXPECT_EQ(FILE_ERROR_ACCESS_DENIED, resource_metadata_->RemoveEntry(
      util::kDriveGrandRootLocalId));
}

TEST_F(ResourceMetadataTest, GetResourceEntryById_RootDirectory) {
  // Look up the root directory by its ID.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      util::kDriveGrandRootLocalId, &entry));
  EXPECT_EQ("drive", entry.base_name());
}

TEST_F(ResourceMetadataTest, GetResourceEntryById) {
  // Get file4 by path.
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &local_id));

  // Confirm that an existing file is found.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      local_id, &entry));
  EXPECT_EQ("file4", entry.base_name());

  // Confirm that a non existing file is not found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryById(
      "file:non_existing", &entry));
}

TEST_F(ResourceMetadataTest, Iterate) {
  scoped_ptr<ResourceMetadata::Iterator> it = resource_metadata_->GetIterator();
  ASSERT_TRUE(it);

  int file_count = 0, directory_count = 0;
  for (; !it->IsAtEnd(); it->Advance()) {
    if (!it->GetValue().file_info().is_directory())
      ++file_count;
    else
      ++directory_count;
  }

  EXPECT_EQ(7, file_count);
  EXPECT_EQ(6, directory_count);
}

TEST_F(ResourceMetadataTest, DuplicatedNames) {
  std::string root_local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root"), &root_local_id));

  ResourceEntry entry;

  // When multiple entries with the same title are added in a single directory,
  // their base_names are de-duped.
  // - drive/root/foo
  // - drive/root/foo (1)
  std::string dir_id_0;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntryWithResourceId(
          "foo", "foo0", root_local_id), &dir_id_0));
  std::string dir_id_1;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntryWithResourceId(
          "foo", "foo1", root_local_id), &dir_id_1));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir_id_0, &entry));
  EXPECT_EQ("foo", entry.base_name());
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir_id_1, &entry));
  EXPECT_EQ("foo (1)", entry.base_name());

  // - drive/root/foo/bar.txt
  // - drive/root/foo/bar (1).txt
  // - drive/root/foo/bar (2).txt
  std::string file_id_0;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId(
          "bar.txt", "bar0", dir_id_0), &file_id_0));
  std::string file_id_1;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId(
          "bar.txt", "bar1", dir_id_0), &file_id_1));
  std::string file_id_2;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId(
          "bar.txt", "bar2", dir_id_0), &file_id_2));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id_0, &entry));
  EXPECT_EQ("bar.txt", entry.base_name());
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id_1, &entry));
  EXPECT_EQ("bar (1).txt", entry.base_name());
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id_2, &entry));
  EXPECT_EQ("bar (2).txt", entry.base_name());

  // Same name but different parent. No renaming.
  // - drive/root/foo (1)/bar.txt
  std::string file_id_3;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId(
          "bar.txt", "bar3", dir_id_1), &file_id_3));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id_3, &entry));
  EXPECT_EQ("bar.txt", entry.base_name());

  // Checks that the entries can be looked up by the de-duped paths.
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/foo/bar (2).txt"), &entry));
  EXPECT_EQ("bar2", entry.resource_id());
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/foo (1)/bar.txt"), &entry));
  EXPECT_EQ("bar3", entry.resource_id());
}

TEST_F(ResourceMetadataTest, EncodedNames) {
  std::string root_local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root"), &root_local_id));

  ResourceEntry entry;

  std::string dir_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("\\(^o^)/", root_local_id), &dir_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir_id, &entry));
  EXPECT_EQ("\\(^o^)\xE2\x88\x95", entry.base_name());

  std::string file_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId("Slash /.txt", "myfile", dir_id),
      &file_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id, &entry));
  EXPECT_EQ("Slash \xE2\x88\x95.txt", entry.base_name());

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe(
          "drive/root/\\(^o^)\xE2\x88\x95/Slash \xE2\x88\x95.txt"),
      &entry));
  EXPECT_EQ("myfile", entry.resource_id());
}

TEST_F(ResourceMetadataTest, Reset) {
  // The grand root has "root" which is not empty.
  std::vector<ResourceEntry> entries;
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/root"), &entries));
  ASSERT_FALSE(entries.empty());

  // Reset.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->Reset());

  // change stamp should be reset.
  EXPECT_EQ(0, resource_metadata_->GetLargestChangestamp());

  // root should continue to exist.
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryByPath(
                base::FilePath::FromUTF8Unsafe("drive"), &entry));
  EXPECT_EQ("drive", entry.base_name());
  ASSERT_TRUE(entry.file_info().is_directory());
  EXPECT_EQ(util::kDriveGrandRootLocalId, entry.resource_id());

  // There is "other" under "drive".
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive"), &entries));
  EXPECT_EQ(1U, entries.size());

  // The "other" directory should be empty.
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/other"), &entries));
  EXPECT_TRUE(entries.empty());
}

}  // namespace internal
}  // namespace drive
