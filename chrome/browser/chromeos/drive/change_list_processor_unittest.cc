// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_processor.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

const int64 kBaseResourceListChangestamp = 123;
const char kBaseResourceListFile[] = "chromeos/gdata/root_feed.json";

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
  ChangeListProcessorTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    metadata_.reset(new internal::ResourceMetadata(temp_dir_.path(),
                                                   blocking_task_runner_));

    FileError error = FILE_ERROR_FAILED;
    metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);
  }

  virtual void TearDown() OVERRIDE {
    metadata_.reset();
    blocking_task_runner_ = NULL;
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
  void ApplyFullResourceList(ScopedVector<ChangeList> changes) {
    scoped_ptr<google_apis::AboutResource> about_resource(
        new google_apis::AboutResource);
    about_resource->set_largest_change_id(kBaseResourceListChangestamp);
    about_resource->set_root_folder_id("fake_root");

    ChangeListProcessor processor(metadata_.get());
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ChangeListProcessor::Apply,
                   base::Unretained(&processor),
                   base::Passed(&about_resource),
                   base::Passed(&changes),
                   false));  // is_delta_update
    google_apis::test_util::RunBlockingPoolTask();
  }

  // Applies the |changes| to |metadata_| as a delta update. Delta changeslists
  // should contain their changestamp in themselves.
  void ApplyChangeList(ScopedVector<ChangeList> changes) {
    scoped_ptr<google_apis::AboutResource> null_about_resource;

    ChangeListProcessor processor(metadata_.get());
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ChangeListProcessor::Apply,
                   base::Unretained(&processor),
                   base::Passed(&null_about_resource),
                   base::Passed(&changes),
                   true));  // is_delta_update
    google_apis::test_util::RunBlockingPoolTask();
  }

  // Gets the resource entry for the path from |metadata_| synchronously.
  // Returns null if the entry does not exist.
  scoped_ptr<ResourceEntry> GetResourceEntry(const std::string& path) {
    FileError error = FILE_ERROR_FAILED;
    scoped_ptr<ResourceEntry> entry(new ResourceEntry);
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_,
        FROM_HERE,
        base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                   base::Unretained(metadata_.get()),
                   base::FilePath::FromUTF8Unsafe(path),
                   entry.get()),
        base::Bind(google_apis::test_util::CreateCopyResultCallback(&error)));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != FILE_ERROR_OK)
      entry.reset();
    return entry.Pass();
  }

  // Gets the largest changestamp stored in |metadata_|.
  int64 GetChangestamp() {
    int64 changestamp = -1;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_,
        FROM_HERE,
        base::Bind(&internal::ResourceMetadata::GetLargestChangestamp,
                   base::Unretained(metadata_.get())),
        base::Bind(google_apis::test_util::CreateCopyResultCallback(
            &changestamp)));
    google_apis::test_util::RunBlockingPoolTask();
    return changestamp;
  }

 private:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<internal::ResourceMetadata, test_util::DestroyHelperForTests>
      metadata_;
};

}  // namespace

TEST_F(ChangeListProcessorTest, ApplyFullResourceList) {
  ApplyFullResourceList(ParseChangeList(kBaseResourceListFile));

  const EntryExpectation kExpected[] = {
      // Root files
      {"drive/root",
          "fake_root", util::kDriveGrandRootSpecialResourceId, DIRECTORY},
      {"drive/root/File 1.txt",
          "file:2_file_resource_id", "fake_root", FILE},
      {"drive/root/Slash \xE2\x88\x95 in file 1.txt",
          "file:slash_file_resource_id", "fake_root", FILE},
      {"drive/root/Document 1 excludeDir-test.gdoc",
          "document:5_document_resource_id", "fake_root", FILE},
      // Subdirectory files
      {"drive/root/Directory 1",
          "folder:1_folder_resource_id", "fake_root", DIRECTORY},
      {"drive/root/Directory 1/SubDirectory File 1.txt",
          "file:subdirectory_file_1_id", "folder:1_folder_resource_id", FILE},
      {"drive/root/Directory 1/Shared To The Account Owner.txt",
          "file:subdirectory_unowned_file_1_id",
          "folder:1_folder_resource_id", FILE},
      {"drive/root/Directory 2 excludeDir-test",
          "folder:sub_dir_folder_2_self_link", "fake_root", DIRECTORY},
      {"drive/root/Slash \xE2\x88\x95 in directory",
          "folder:slash_dir_folder_resource_id", "fake_root", DIRECTORY},
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
    EXPECT_EQ(kExpected[i].parent_id, entry->parent_resource_id());
    EXPECT_EQ(kExpected[i].type,
              entry->file_info().is_directory() ? DIRECTORY : FILE);
  }

  EXPECT_EQ(kBaseResourceListChangestamp, GetChangestamp());
}

TEST_F(ChangeListProcessorTest, DeltaFileAddedInNewDirectory) {
  const char kTestJson[] =
      "chromeos/gdata/delta_file_added_in_new_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kRootId("fake_root");
  const std::string kNewFolderId("folder:new_folder_resource_id");
  const std::string kNewFileId("document:file_added_in_new_dir_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kNewFolderId));
  EXPECT_TRUE(entry_map.count(kNewFileId));
  EXPECT_EQ(kRootId, entry_map[kNewFolderId].parent_resource_id());
  EXPECT_EQ(kNewFolderId, entry_map[kNewFileId].parent_resource_id());
  EXPECT_TRUE(entry_map[kNewFolderId].file_info().is_directory());
  EXPECT_FALSE(entry_map[kNewFileId].file_info().is_directory());
  EXPECT_EQ("New Directory", entry_map[kNewFolderId].title());
  EXPECT_EQ("File in new dir", entry_map[kNewFileId].title());

  // Apply the changelist and check the effect.
  ApplyFullResourceList(ParseChangeList(kBaseResourceListFile));
  ApplyChangeList(ParseChangeList(kTestJson));

  EXPECT_EQ(16730, GetChangestamp());  // the value is written in kTestJson.
  EXPECT_TRUE(GetResourceEntry("drive/root/New Directory"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/New Directory/File in new dir.gdoc"));
}

TEST_F(ChangeListProcessorTest, DeltaDirMovedFromRootToDirectory) {
  const char kTestJson[] =
      "chromeos/gdata/delta_dir_moved_from_root_to_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kMovedId("folder:1_folder_resource_id");
  const std::string kDestId("folder:sub_dir_folder_2_self_link");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kMovedId));
  EXPECT_TRUE(entry_map.count(kDestId));
  EXPECT_EQ(kDestId, entry_map[kMovedId].parent_resource_id());

  // Apply the changelist and check the effect.
  ApplyFullResourceList(ParseChangeList(kBaseResourceListFile));
  ApplyChangeList(ParseChangeList(kTestJson));

  EXPECT_EQ(16809, GetChangestamp());  // the value is written in kTestJson.
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 2 excludeDir-test/Directory 1"));
}

TEST_F(ChangeListProcessorTest, DeltaFileMovedFromDirectoryToRoot) {
  const char kTestJson[] =
      "chromeos/gdata/delta_file_moved_from_directory_to_root.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kRootId("fake_root");
  const std::string kMovedId("file:subdirectory_file_1_id");
  const std::string kSrcId("folder:1_folder_resource_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kMovedId));
  EXPECT_TRUE(entry_map.count(kSrcId));
  EXPECT_EQ(kRootId, entry_map[kMovedId].parent_resource_id());

  // Apply the changelist and check the effect.
  ApplyFullResourceList(ParseChangeList(kBaseResourceListFile));
  ApplyChangeList(ParseChangeList(kTestJson));

  EXPECT_EQ(16815, GetChangestamp());  // the value is written in kTestJson.
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry("drive/root/SubDirectory File 1.txt"));
}

TEST_F(ChangeListProcessorTest, DeltaFileRenamedInDirectory) {
  const char kTestJson[] =
      "chromeos/gdata/delta_file_renamed_in_directory.json";

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::ConvertToMap(
      ParseChangeList(kTestJson), &entry_map, NULL);

  const std::string kRootId("fake_root");
  const std::string kRenamedId("file:subdirectory_file_1_id");
  const std::string kParentId("folder:1_folder_resource_id");

  // Check the content of parsed ResourceEntryMap.
  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kRenamedId));
  EXPECT_TRUE(entry_map.count(kParentId));
  EXPECT_EQ(kParentId, entry_map[kRenamedId].parent_resource_id());
  EXPECT_EQ("New SubDirectory File 1.txt", entry_map[kRenamedId].title());

  // Apply the changelist and check the effect.
  ApplyFullResourceList(ParseChangeList(kBaseResourceListFile));
  ApplyChangeList(ParseChangeList(kTestJson));

  EXPECT_EQ(16767, GetChangestamp());  // the value is written in kTestJson.
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 1/New SubDirectory File 1.txt"));
}

// TODO(kinaba): add test for all patterns in test/data/chromeos/gdata
// http://crbug.com/147728.

}  // namespace internal
}  // namespace drive
