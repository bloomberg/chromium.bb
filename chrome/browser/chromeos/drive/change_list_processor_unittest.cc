// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_processor.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

// Parses a json file at |test_data_path| relative to Chrome test directory into
// a drive::internal::ChangeList structure.
ChangeList* ParseChangeList(const std::string& test_data_path) {
  return new ChangeList(
      *google_apis::ResourceList::ExtractAndParse(
          *google_apis::test_util::LoadJSONFile(
              test_data_path)));
}

}  // namespace

TEST(ChangeListProcessorTest, DeltaFileAddedInNewDirectory) {
  ScopedVector<ChangeList> changes;
  changes.push_back(ParseChangeList(
      "chromeos/gdata/delta_file_added_in_new_directory.json"));

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::FeedToEntryMap(changes.Pass(), &entry_map, NULL);

  const std::string kRootId("fake_root");
  const std::string kNewFolderId("folder:new_folder_resource_id");
  const std::string kNewFileId("document:file_added_in_new_dir_id");

  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kNewFolderId));
  EXPECT_TRUE(entry_map.count(kNewFileId));
  EXPECT_EQ(kRootId, entry_map[kNewFolderId].parent_resource_id());
  EXPECT_EQ(kNewFolderId, entry_map[kNewFileId].parent_resource_id());
  EXPECT_TRUE(entry_map[kNewFolderId].file_info().is_directory());
  EXPECT_FALSE(entry_map[kNewFileId].file_info().is_directory());
  EXPECT_EQ("New Directory", entry_map[kNewFolderId].title());
  EXPECT_EQ("File in new dir", entry_map[kNewFileId].title());
}

TEST(ChangeListProcessorTest, DeltaDirMovedFromRootToDirectory) {
  ScopedVector<ChangeList> changes;
  changes.push_back(ParseChangeList(
      "chromeos/gdata/delta_dir_moved_from_root_to_directory.json"));

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::FeedToEntryMap(changes.Pass(), &entry_map, NULL);

  const std::string kMovedId("folder:1_folder_resource_id");
  const std::string kDestId("folder:sub_dir_folder_2_self_link");

  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kMovedId));
  EXPECT_TRUE(entry_map.count(kDestId));
  EXPECT_EQ(kDestId, entry_map[kMovedId].parent_resource_id());
}

TEST(ChangeListProcessorTest, DeltaFileMovedFromDirectoryToRoot) {
  ScopedVector<ChangeList> changes;
  changes.push_back(ParseChangeList(
      "chromeos/gdata/delta_file_moved_from_directory_to_root.json"));

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::FeedToEntryMap(changes.Pass(), &entry_map, NULL);

  const std::string kRootId("fake_root");
  const std::string kMovedId("file:subdirectory_file_1_id");
  const std::string kSrcId("folder:1_folder_resource_id");

  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kMovedId));
  EXPECT_TRUE(entry_map.count(kSrcId));
  EXPECT_EQ(kRootId, entry_map[kMovedId].parent_resource_id());
}

TEST(ChangeListProcessorTest, DeltaFileRenamedInDirectory) {
  ScopedVector<ChangeList> changes;
  changes.push_back(ParseChangeList(
      "chromeos/gdata/delta_file_renamed_in_directory.json"));

  ChangeListProcessor::ResourceEntryMap entry_map;
  ChangeListProcessor::FeedToEntryMap(changes.Pass(), &entry_map, NULL);

  const std::string kRootId("fake_root");
  const std::string kRenamedId("file:subdirectory_file_1_id");
  const std::string kParentId("folder:1_folder_resource_id");

  EXPECT_EQ(2U, entry_map.size());
  EXPECT_TRUE(entry_map.count(kRenamedId));
  EXPECT_TRUE(entry_map.count(kParentId));
  EXPECT_EQ(kParentId, entry_map[kRenamedId].parent_resource_id());
  EXPECT_EQ("New SubDirectory File 1.txt", entry_map[kRenamedId].title());
}

// TODO(kinaba): write test for ApplyFeeds crbug.com/147728.

}  // namespace internal
}  // namespace drive

