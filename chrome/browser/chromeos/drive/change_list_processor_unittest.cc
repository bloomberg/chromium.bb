// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_processor.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

const int64 kBaseResourceListChangestamp = 123;
const char kRootId[] = "fake_root";

enum FileOrDirectory {
  FILE,
  DIRECTORY,
};

struct EntryExpectation {
  std::string path;
  std::string id;
  std::string parent_id;
  FileOrDirectory type;
};

// Returns a basic change list which contains some files and directories.
ScopedVector<ChangeList> CreateBaseChangeList() {
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  // Add directories to the change list.
  ResourceEntry directory;
  directory.mutable_file_info()->set_is_directory(true);

  directory.set_title("Directory 1");
  directory.set_resource_id("folder:1_folder_resource_id");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  directory.set_title("Sub Directory Folder");
  directory.set_resource_id("folder:sub_dir_folder_resource_id");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:1_folder_resource_id");

  directory.set_title("Sub Sub Directory Folder");
  directory.set_resource_id("folder:sub_sub_directory_folder_id");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:sub_dir_folder_resource_id");

  directory.set_title("Directory 2 excludeDir-test");
  directory.set_resource_id("folder:sub_dir_folder_2_self_link");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  // Add files to the change list.
  ResourceEntry file;

  file.set_title("File 1.txt");
  file.set_resource_id("file:2_file_resource_id");
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  file.set_title("SubDirectory File 1.txt");
  file.set_resource_id("file:subdirectory_file_1_id");
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:1_folder_resource_id");

  file.set_title("Orphan File 1.txt");
  file.set_resource_id("file:1_orphanfile_resource_id");
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back("");

  change_lists[0]->set_largest_changestamp(kBaseResourceListChangestamp);
  return change_lists.Pass();
}

class ChangeListProcessorTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);
    cache_.reset(new FileCache(metadata_storage_.get(),
                               temp_dir_.path(),
                               base::MessageLoopProxy::current().get(),
                               fake_free_disk_space_getter_.get()));
    ASSERT_TRUE(cache_->Initialize());

    metadata_.reset(new internal::ResourceMetadata(
        metadata_storage_.get(), cache_.get(),
        base::MessageLoopProxy::current()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());
  }

  // Applies the |changes| to |metadata_| as a full resource list of changestamp
  // |kBaseResourceListChangestamp|.
  FileError ApplyFullResourceList(ScopedVector<ChangeList> changes) {
    scoped_ptr<google_apis::AboutResource> about_resource(
        new google_apis::AboutResource);
    about_resource->set_largest_change_id(kBaseResourceListChangestamp);
    about_resource->set_root_folder_id(kRootId);

    ChangeListProcessor processor(metadata_.get());
    return processor.Apply(about_resource.Pass(),
                           changes.Pass(),
                           false /* is_delta_update */);
  }

  // Applies the |changes| to |metadata_| as a delta update. Delta changelists
  // should contain their changestamp in themselves.
  FileError ApplyChangeList(ScopedVector<ChangeList> changes,
                            FileChange* changed_files) {
    scoped_ptr<google_apis::AboutResource> about_resource(
        new google_apis::AboutResource);
    about_resource->set_largest_change_id(kBaseResourceListChangestamp);
    about_resource->set_root_folder_id(kRootId);

    ChangeListProcessor processor(metadata_.get());
    FileError error = processor.Apply(about_resource.Pass(),
                                      changes.Pass(),
                                      true /* is_delta_update */);
    *changed_files = processor.changed_files();
    return error;
  }

  // Gets the resource entry for the path from |metadata_| synchronously.
  // Returns null if the entry does not exist.
  scoped_ptr<ResourceEntry> GetResourceEntry(const std::string& path) {
    scoped_ptr<ResourceEntry> entry(new ResourceEntry);
    FileError error = metadata_->GetResourceEntryByPath(
        base::FilePath::FromUTF8Unsafe(path), entry.get());
    if (error != FILE_ERROR_OK)
      entry.reset();
    return entry.Pass();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<ResourceMetadataStorage,
             test_util::DestroyHelperForTests> metadata_storage_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
};

}  // namespace

TEST_F(ChangeListProcessorTest, ApplyFullResourceList) {
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  const EntryExpectation kExpected[] = {
      // Root files
      {"drive/root", kRootId, "", DIRECTORY},
      {"drive/root/File 1.txt",
          "file:2_file_resource_id", kRootId, FILE},
      // Subdirectory files
      {"drive/root/Directory 1",
          "folder:1_folder_resource_id", kRootId, DIRECTORY},
      {"drive/root/Directory 1/SubDirectory File 1.txt",
          "file:subdirectory_file_1_id", "folder:1_folder_resource_id", FILE},
      {"drive/root/Directory 2 excludeDir-test",
          "folder:sub_dir_folder_2_self_link", kRootId, DIRECTORY},
      // Deeper
      {"drive/root/Directory 1/Sub Directory Folder",
          "folder:sub_dir_folder_resource_id",
          "folder:1_folder_resource_id", DIRECTORY},
      {"drive/root/Directory 1/Sub Directory Folder/Sub Sub Directory Folder",
          "folder:sub_sub_directory_folder_id",
          "folder:sub_dir_folder_resource_id", DIRECTORY},
      // Orphan
      {"drive/other/Orphan File 1.txt", "file:1_orphanfile_resource_id",
           "", FILE},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExpected); ++i) {
    scoped_ptr<ResourceEntry> entry = GetResourceEntry(kExpected[i].path);
    ASSERT_TRUE(entry) << "for path: " << kExpected[i].path;
    EXPECT_EQ(kExpected[i].id, entry->resource_id());

    ResourceEntry parent_entry;
    EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryById(
        entry->parent_local_id(), &parent_entry));
    EXPECT_EQ(kExpected[i].parent_id, parent_entry.resource_id());
    EXPECT_EQ(kExpected[i].type,
              entry->file_info().is_directory() ? DIRECTORY : FILE);
  }

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(kBaseResourceListChangestamp, changestamp);
}

TEST_F(ChangeListProcessorTest, DeltaFileAddedInNewDirectory) {
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry new_folder;
  new_folder.set_resource_id("folder:new_folder_resource_id");
  new_folder.set_title("New Directory");
  new_folder.mutable_file_info()->set_is_directory(true);
  change_lists[0]->mutable_entries()->push_back(new_folder);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  ResourceEntry new_file;
  new_file.set_resource_id("document:file_added_in_new_dir_id");
  new_file.set_title("File in new dir.txt");
  change_lists[0]->mutable_entries()->push_back(new_file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      new_folder.resource_id());

  change_lists[0]->set_largest_changestamp(16730);

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16730, changestamp);
  EXPECT_TRUE(GetResourceEntry("drive/root/New Directory"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/New Directory/File in new dir.txt"));

  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/New Directory/File in new dir.txt")));
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/New Directory")));
}

TEST_F(ChangeListProcessorTest, DeltaDirMovedFromRootToDirectory) {
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry entry;
  entry.set_resource_id("folder:1_folder_resource_id");
  entry.set_title("Directory 1");
  entry.mutable_file_info()->set_is_directory(true);
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:sub_dir_folder_2_self_link");

  change_lists[0]->set_largest_changestamp(16809);

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16809, changestamp);
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 2 excludeDir-test/Directory 1"));

  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.CountDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
  EXPECT_TRUE(changed_files.CountDirectory(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 2 excludeDir-test")));
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 2 excludeDir-test/Directory 1")));
}

TEST_F(ChangeListProcessorTest, DeltaFileMovedFromDirectoryToRoot) {
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry entry;
  entry.set_resource_id("file:subdirectory_file_1_id");
  entry.set_title("SubDirectory File 1.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_largest_changestamp(16815);

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16815, changestamp);
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry("drive/root/SubDirectory File 1.txt"));

  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/SubDirectory File 1.txt")));
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 1/SubDirectory File 1.txt")));
}

TEST_F(ChangeListProcessorTest, DeltaFileRenamedInDirectory) {
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry entry;
  entry.set_resource_id("file:subdirectory_file_1_id");
  entry.set_title("New SubDirectory File 1.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:1_folder_resource_id");

  change_lists[0]->set_largest_changestamp(16767);

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16767, changestamp);
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 1/New SubDirectory File 1.txt"));

  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 1/SubDirectory File 1.txt")));
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 1/New SubDirectory File 1.txt")));
}

TEST_F(ChangeListProcessorTest, DeltaAddAndDeleteFileInRoot) {
  // Create ChangeList to add a file.
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry entry;
  entry.set_resource_id("document:added_in_root_id");
  entry.set_title("Added file.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_largest_changestamp(16683);

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16683, changestamp);
  EXPECT_TRUE(GetResourceEntry("drive/root/Added file.txt"));
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Added file.txt")));

  // Create ChangeList to delete the file.
  change_lists.push_back(new ChangeList);

  entry.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_largest_changestamp(16687);

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16687, changestamp);
  EXPECT_FALSE(GetResourceEntry("drive/root/Added file.txt"));
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Added file.txt")));
}


TEST_F(ChangeListProcessorTest, DeltaAddAndDeleteFileFromExistingDirectory) {
  // Create ChangeList to add a file.
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry entry;
  entry.set_resource_id("document:added_in_root_id");
  entry.set_title("Added file.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:1_folder_resource_id");

  change_lists[0]->set_largest_changestamp(16730);

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));
  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16730, changestamp);
  EXPECT_TRUE(GetResourceEntry("drive/root/Directory 1/Added file.txt"));

  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1/Added file.txt")));

  // Create ChangeList to delete the file.
  change_lists.push_back(new ChangeList);

  entry.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:1_folder_resource_id");

  change_lists[0]->set_largest_changestamp(16770);

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16770, changestamp);
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1/Added file.txt"));

  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1/Added file.txt")));
}

TEST_F(ChangeListProcessorTest, DeltaAddFileToNewButDeletedDirectory) {
  // Create a change which contains the following updates:
  // 1) A new PDF file is added to a new directory
  // 2) but the new directory is marked "deleted" (i.e. moved to Trash)
  // Hence, the PDF file should be just ignored.
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  ResourceEntry file;
  file.set_resource_id("pdf:file_added_in_deleted_id");
  file.set_title("new_pdf_file.pdf");
  file.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "folder:new_folder_resource_id");

  ResourceEntry directory;
  directory.set_resource_id("folder:new_folder_resource_id");
  directory.set_title("New Directory");
  directory.mutable_file_info()->set_is_directory(true);
  directory.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_largest_changestamp(16730);

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(16730, changestamp);
  EXPECT_FALSE(GetResourceEntry("drive/root/New Directory/new_pdf_file.pdf"));

  EXPECT_TRUE(changed_files.empty());
}

TEST_F(ChangeListProcessorTest, RefreshDirectory) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change list.
  scoped_ptr<ChangeList> change_list(new ChangeList);

  // Add a new file to the change list.
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  change_list->mutable_entries()->push_back(new_file);
  change_list->mutable_parent_resource_ids()->push_back(kRootId);

  // Add "Directory 1" to the map with a new name.
  ResourceEntry dir1;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII("Directory 1"), &dir1));
  dir1.set_title(dir1.title() + " (renamed)");
  change_list->mutable_entries()->push_back(dir1);
  change_list->mutable_parent_resource_ids()->push_back(kRootId);

  // Update the directory with the map.
  ResourceEntry root;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root));
  const int64 kNewChangestamp = 12345;
  ResourceEntryVector refreshed_entries;
  EXPECT_EQ(FILE_ERROR_OK, ChangeListProcessor::RefreshDirectory(
      metadata_.get(),
      DirectoryFetchInfo(root.local_id(), kRootId, kNewChangestamp),
      change_list.Pass(),
      &refreshed_entries));

  // "new_file" should be added.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(new_file.title()), &entry));

  // "Directory 1" should be renamed.
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(dir1.title()), &entry));
}

TEST_F(ChangeListProcessorTest, RefreshDirectory_WrongParentId) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change list and add a new file to it.
  scoped_ptr<ChangeList> change_list(new ChangeList);
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  // This entry should not be added because the parent ID does not match.
  change_list->mutable_parent_resource_ids()->push_back(
      "some-random-resource-id");
  change_list->mutable_entries()->push_back(new_file);


  // Update the directory.
  ResourceEntry root;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root));
  const int64 kNewChangestamp = 12345;
  ResourceEntryVector refreshed_entries;
  EXPECT_EQ(FILE_ERROR_OK, ChangeListProcessor::RefreshDirectory(
      metadata_.get(),
      DirectoryFetchInfo(root.local_id(), kRootId, kNewChangestamp),
      change_list.Pass(),
      &refreshed_entries));

  // "new_file" should not be added.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(new_file.title()), &entry));
}

TEST_F(ChangeListProcessorTest, SharedFilesWithNoParentInFeed) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change lists.
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  // Add a new file with non-existing parent resource id to the change lists.
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  change_lists[0]->mutable_entries()->push_back(new_file);
  change_lists[0]->mutable_parent_resource_ids()->push_back("nonexisting");
  change_lists[0]->set_largest_changestamp(kBaseResourceListChangestamp + 1);

  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  // "new_file" should be added under drive/other.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveGrandRootPath().AppendASCII("other/new_file"), &entry));
}

TEST_F(ChangeListProcessorTest, ModificationDate) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change lists with a new file.
  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList);

  const base::Time now = base::Time::Now();
  ResourceEntry new_file_remote;
  new_file_remote.set_title("new_file_remote");
  new_file_remote.set_resource_id("new_file_id");
  new_file_remote.set_modification_date(now.ToInternalValue());

  change_lists[0]->mutable_entries()->push_back(new_file_remote);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);
  change_lists[0]->set_largest_changestamp(kBaseResourceListChangestamp + 1);

  // Add the same file locally, but with a different name, a dirty metadata
  // state, and a newer modification date.
  ResourceEntry root;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root));

  ResourceEntry new_file_local;
  new_file_local.set_resource_id(new_file_remote.resource_id());
  new_file_local.set_parent_local_id(root.local_id());
  new_file_local.set_title("new_file_local");
  new_file_local.set_metadata_edit_state(ResourceEntry::DIRTY);
  new_file_local.set_modification_date(
      (now + base::TimeDelta::FromSeconds(1)).ToInternalValue());
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->AddEntry(new_file_local, &local_id));

  // Apply the change.
  FileChange changed_files;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(change_lists.Pass(), &changed_files));

  // The change is rejected due to the old modification date.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryById(local_id, &entry));
  EXPECT_EQ(new_file_local.title(), entry.title());
}

}  // namespace internal
}  // namespace drive
