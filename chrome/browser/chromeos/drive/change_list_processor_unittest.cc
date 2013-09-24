// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_processor.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

const int64 kBaseResourceListChangestamp = 123;
const char kBaseResourceListFile[] = "gdata/root_feed.json";
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

class ChangeListProcessorTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    metadata_.reset(new internal::ResourceMetadata(
        metadata_storage_.get(), base::MessageLoopProxy::current()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());
  }

  // Parses a json file at |test_data_path| relative to Chrome test directory
  // into a ScopedVector<drive::internal::ChangeList>.
  ScopedVector<ChangeList> ParseChangeList(const std::string& test_data_path) {
    ScopedVector<ChangeList> changes;
    changes.push_back(new ChangeList(
        *google_apis::ResourceList::ExtractAndParse(
            *google_apis::test_util::LoadJSONFile(
                test_data_path))));
    return changes.Pass();
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
                            std::set<base::FilePath>* changed_dirs) {
    ChangeListProcessor processor(metadata_.get());
    FileError error = processor.Apply(scoped_ptr<google_apis::AboutResource>(),
                                      changes.Pass(),
                                      true /* is_delta_update */);
    *changed_dirs = processor.changed_dirs();
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
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
};

}  // namespace

TEST_F(ChangeListProcessorTest, ApplyFullResourceList) {
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));

  const EntryExpectation kExpected[] = {
      // Root files
      {"drive/root",
          kRootId, util::kDriveGrandRootSpecialResourceId, DIRECTORY},
      {"drive/root/File 1.txt",
          "file:2_file_resource_id", kRootId, FILE},
      {"drive/root/Slash \xE2\x88\x95 in file 1.txt",
          "file:slash_file_resource_id", kRootId, FILE},
      {"drive/root/Document 1 excludeDir-test.gdoc",
          "document:5_document_resource_id", kRootId, FILE},
      // Subdirectory files
      {"drive/root/Directory 1",
          "folder:1_folder_resource_id", kRootId, DIRECTORY},
      {"drive/root/Directory 1/SubDirectory File 1.txt",
          "file:subdirectory_file_1_id", "folder:1_folder_resource_id", FILE},
      {"drive/root/Directory 1/Shared To The Account Owner.txt",
          "file:subdirectory_unowned_file_1_id",
          "folder:1_folder_resource_id", FILE},
      {"drive/root/Directory 2 excludeDir-test",
          "folder:sub_dir_folder_2_self_link", kRootId, DIRECTORY},
      {"drive/root/Slash \xE2\x88\x95 in directory",
          "folder:slash_dir_folder_resource_id", kRootId, DIRECTORY},
      {"drive/root/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt",
          "file:slash_subdir_file",
          "folder:slash_dir_folder_resource_id", FILE},
      // Deeper
      {"drive/root/Directory 1/Sub Directory Folder",
          "folder:sub_dir_folder_resource_id",
          "folder:1_folder_resource_id", DIRECTORY},
      {"drive/root/Directory 1/Sub Directory Folder/Sub Sub Directory Folder",
          "folder:sub_sub_directory_folder_id",
          "folder:sub_dir_folder_resource_id", DIRECTORY},
      // Orphan
      {"drive/other/Orphan File 1.txt",
          "file:1_orphanfile_resource_id",
          util::kDriveOtherDirSpecialResourceId, FILE},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExpected); ++i) {
    scoped_ptr<ResourceEntry> entry = GetResourceEntry(kExpected[i].path);
    ASSERT_TRUE(entry) << "for path: " << kExpected[i].path;
    EXPECT_EQ(kExpected[i].id, entry->resource_id());
    EXPECT_EQ(kExpected[i].parent_id, entry->parent_local_id());
    EXPECT_EQ(kExpected[i].type,
              entry->file_info().is_directory() ? DIRECTORY : FILE);
  }

  EXPECT_EQ(kBaseResourceListChangestamp, metadata_->GetLargestChangestamp());
}

TEST_F(ChangeListProcessorTest, DeltaFileAddedInNewDirectory) {
  const char kTestJson[] =
      "gdata/delta_file_added_in_new_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kNewFolderId("folder:new_folder_resource_id");
  const std::string kNewFileId("document:file_added_in_new_dir_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kNewFolderId));
  EXPECT_TRUE(entry_map.count(kNewFileId));
  EXPECT_EQ(kRootId, entry_map[kNewFolderId].parent_local_id());
  EXPECT_EQ(kNewFolderId, entry_map[kNewFileId].parent_local_id());
  EXPECT_TRUE(entry_map[kNewFolderId].file_info().is_directory());
  EXPECT_FALSE(entry_map[kNewFileId].file_info().is_directory());
  EXPECT_EQ("New Directory", entry_map[kNewFolderId].title());
  EXPECT_EQ("File in new dir", entry_map[kNewFileId].title());

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJson), &changed_dirs));

  // The value is written in kTestJson.
  EXPECT_EQ(16730, metadata_->GetLargestChangestamp());
  EXPECT_TRUE(GetResourceEntry("drive/root/New Directory"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/New Directory/File in new dir.gdoc"));

  EXPECT_EQ(2U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root/New Directory")));
}

TEST_F(ChangeListProcessorTest, DeltaDirMovedFromRootToDirectory) {
  const char kTestJson[] =
      "gdata/delta_dir_moved_from_root_to_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kMovedId("folder:1_folder_resource_id");
  const std::string kDestId("folder:sub_dir_folder_2_self_link");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kMovedId));
  EXPECT_TRUE(entry_map.count(kDestId));
  EXPECT_EQ(kDestId, entry_map[kMovedId].parent_local_id());

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJson), &changed_dirs));

  // The value is written in kTestJson.
  EXPECT_EQ(16809, metadata_->GetLargestChangestamp());
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 2 excludeDir-test/Directory 1"));

  EXPECT_EQ(4U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe(
          "drive/root/Directory 2 excludeDir-test")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe(
          "drive/root/Directory 2 excludeDir-test/Directory 1")));
}

TEST_F(ChangeListProcessorTest, DeltaFileMovedFromDirectoryToRoot) {
  const char kTestJson[] =
      "gdata/delta_file_moved_from_directory_to_root.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kMovedId("file:subdirectory_file_1_id");
  const std::string kSrcId("folder:1_folder_resource_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kMovedId));
  EXPECT_TRUE(entry_map.count(kSrcId));
  EXPECT_EQ(kRootId, entry_map[kMovedId].parent_local_id());

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJson), &changed_dirs));

  // The value is written in kTestJson.
  EXPECT_EQ(16815, metadata_->GetLargestChangestamp());
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry("drive/root/SubDirectory File 1.txt"));

  EXPECT_EQ(2U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
}

TEST_F(ChangeListProcessorTest, DeltaFileRenamedInDirectory) {
  const char kTestJson[] =
      "gdata/delta_file_renamed_in_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kRenamedId("file:subdirectory_file_1_id");
  const std::string kParentId("folder:1_folder_resource_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kRenamedId));
  EXPECT_TRUE(entry_map.count(kParentId));
  EXPECT_EQ(kParentId, entry_map[kRenamedId].parent_local_id());
  EXPECT_EQ("New SubDirectory File 1.txt", entry_map[kRenamedId].title());

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJson), &changed_dirs));

  // The value is written in kTestJson.
  EXPECT_EQ(16767, metadata_->GetLargestChangestamp());
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 1/New SubDirectory File 1.txt"));

  EXPECT_EQ(2U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
}

TEST_F(ChangeListProcessorTest, DeltaAddAndDeleteFileInRoot) {
  const char kTestJsonAdd[] =
      "gdata/delta_file_added_in_root.json";
  const char kTestJsonDelete[] =
      "gdata/delta_file_deleted_in_root.json";

  const std::string kParentId(kRootId);
  const std::string kFileId("document:added_in_root_id");

  ChangeListProcessor::ResourceEntryMap entry_map;

  // Check the content of kTestJsonAdd.
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJsonAdd), &entry_map, NULL);
  EXPECT_EQ(1U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kFileId));
  EXPECT_EQ(kParentId, entry_map[kFileId].parent_local_id());
  EXPECT_EQ("Added file", entry_map[kFileId].title());
  EXPECT_FALSE(entry_map[kFileId].deleted());

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJsonAdd), &changed_dirs));
  EXPECT_EQ(16683, metadata_->GetLargestChangestamp());
  EXPECT_TRUE(GetResourceEntry("drive/root/Added file.gdoc"));
  EXPECT_EQ(1U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));

  // Check the content of kTestJsonDelete.
  entry_map.clear();
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJsonDelete), &entry_map, NULL);
  EXPECT_EQ(1U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kFileId));
  EXPECT_EQ(kParentId, entry_map[kFileId].parent_local_id());
  EXPECT_EQ("Added file", entry_map[kFileId].title());
  EXPECT_TRUE(entry_map[kFileId].deleted());

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJsonDelete), &changed_dirs));
  EXPECT_EQ(16687, metadata_->GetLargestChangestamp());
  EXPECT_FALSE(GetResourceEntry("drive/root/Added file.gdoc"));
  EXPECT_EQ(1U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));
}


TEST_F(ChangeListProcessorTest, DeltaAddAndDeleteFileFromExistingDirectory) {
  const char kTestJsonAdd[] =
      "gdata/delta_file_added_in_directory.json";
  const char kTestJsonDelete[] =
      "gdata/delta_file_deleted_in_directory.json";

  const std::string kParentId("folder:1_folder_resource_id");
  const std::string kFileId("document:added_in_root_id");

  ChangeListProcessor::ResourceEntryMap entry_map;

  // Check the content of kTestJsonAdd.
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJsonAdd), &entry_map, NULL);
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kFileId));
  EXPECT_TRUE(entry_map.count(kParentId));
  EXPECT_EQ(kParentId, entry_map[kFileId].parent_local_id());
  EXPECT_EQ("Added file", entry_map[kFileId].title());
  EXPECT_FALSE(entry_map[kFileId].deleted());

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJsonAdd), &changed_dirs));
  EXPECT_EQ(16730, metadata_->GetLargestChangestamp());
  EXPECT_TRUE(GetResourceEntry("drive/root/Directory 1/Added file.gdoc"));

  EXPECT_EQ(2U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));

  // Check the content of kTestJsonDelete.
  entry_map.clear();
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJsonDelete), &entry_map, NULL);
  EXPECT_EQ(1U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kFileId));
  EXPECT_EQ(kParentId, entry_map[kFileId].parent_local_id());
  EXPECT_EQ("Added file", entry_map[kFileId].title());
  EXPECT_TRUE(entry_map[kFileId].deleted());

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJsonDelete), &changed_dirs));
  EXPECT_EQ(16770, metadata_->GetLargestChangestamp());
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1/Added file.gdoc"));

  EXPECT_EQ(1U, changed_dirs.size());
  EXPECT_TRUE(changed_dirs.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
}

TEST_F(ChangeListProcessorTest, DeltaAddFileToNewButDeletedDirectory) {
  // This file contains the following updates:
  // 1) A new PDF file is added to a new directory
  // 2) but the new directory is marked "deleted" (i.e. moved to Trash)
  // Hence, the PDF file should be just ignored.
  const char kTestJson[] =
      "gdata/delta_file_added_in_new_but_deleted_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kDirId("folder:new_folder_resource_id");
  const std::string kFileId("pdf:file_added_in_deleted_dir_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kDirId));
  EXPECT_TRUE(entry_map.count(kFileId));
  EXPECT_EQ(kDirId, entry_map[kFileId].parent_local_id());
  EXPECT_TRUE(entry_map[kDirId].deleted());

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));
  std::set<base::FilePath> changed_dirs;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyChangeList(ParseChangeList(kTestJson), &changed_dirs));

  // The value is written in kTestJson.
  EXPECT_EQ(16730, metadata_->GetLargestChangestamp());
  EXPECT_FALSE(GetResourceEntry("drive/root/New Directory/new_pdf_file.pdf"));

  EXPECT_TRUE(changed_dirs.empty());
}

TEST_F(ChangeListProcessorTest, RefreshDirectory) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));

  // Create a map.
  ChangeListProcessor::ResourceEntryMap entry_map;

  // Add a new file to the map.
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  new_file.set_parent_local_id(kRootId);
  entry_map["new_file_id"] = new_file;

  // Add "Directory 1" to the map with a new name.
  ResourceEntry dir1;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII("Directory 1"), &dir1));
  dir1.set_title(dir1.title() + " (renamed)");
  entry_map[dir1.resource_id()] = dir1;

  // Update the directory with the map.
  const int64 kNewChangestamp = 12345;
  base::FilePath file_path;
  EXPECT_EQ(FILE_ERROR_OK, ChangeListProcessor::RefreshDirectory(
      metadata_.get(),
      DirectoryFetchInfo(kRootId, kNewChangestamp),
      entry_map,
      &file_path));
  EXPECT_EQ(util::GetDriveMyDriveRootPath().value(), file_path.value());

  // The new changestamp should be set.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &entry));
  EXPECT_EQ(kNewChangestamp, entry.directory_specific_info().changestamp());

  // "new_file" should be added.
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(new_file.title()), &entry));

  // "Directory 1" should be renamed.
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(dir1.title()), &entry));
}

TEST_F(ChangeListProcessorTest, RefreshDirectory_WrongParentId) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyFullResourceList(ParseChangeList(kBaseResourceListFile)));

  // Create a map and add a new file to it.
  ChangeListProcessor::ResourceEntryMap entry_map;
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  // This entry should not be added because the parent ID does not match.
  new_file.set_parent_local_id("some-random-resource-id");
  entry_map["new_file_id"] = new_file;

  // Update the directory with the map.
  const int64 kNewChangestamp = 12345;
  base::FilePath file_path;
  EXPECT_EQ(FILE_ERROR_OK, ChangeListProcessor::RefreshDirectory(
      metadata_.get(),
      DirectoryFetchInfo(kRootId, kNewChangestamp),
      entry_map,
      &file_path));
  EXPECT_EQ(util::GetDriveMyDriveRootPath().value(), file_path.value());

  // "new_file" should not be added.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(new_file.title()), &entry));
}

}  // namespace internal
}  // namespace drive
