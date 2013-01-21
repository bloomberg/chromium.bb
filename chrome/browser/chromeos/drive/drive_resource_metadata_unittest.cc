// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

// See drive.proto for the difference between the two URLs.
const char kResumableEditMediaUrl[] = "http://resumable-edit-media/";
const char kResumableCreateMediaUrl[] = "http://resumable-create-media/";

const char kTestRootResourceId[] = "test_root";

// Callback for DriveResourceMetadata::InitFromDB.
void CopyResultsFromInitFromDBCallback(DriveFileError* expected_error,
                                       DriveFileError actual_error) {
  *expected_error = actual_error;
}

// Copies result from GetChildDirectoriesCallback.
void CopyResultFromGetChildDirectoriesCallback(
    std::set<FilePath>* out_child_directories,
    const std::set<FilePath>& in_child_directories) {
  *out_child_directories = in_child_directories;
}

// Copies result from GetChangestampCallback.
void CopyResultFromGetChangestampCallback(
    int64* out_changestamp, int64 in_changestamp) {
  *out_changestamp = in_changestamp;
}

}  // namespace

class DriveResourceMetadataTest : public testing::Test {
 public:
  DriveResourceMetadataTest();

 protected:
  // Creates a DriveEntryProto.
  DriveEntryProto CreateDriveEntryProto(int sequence_id,
                                        bool is_directory,
                                        const std::string& parent_resource_id);

  // Adds a DriveEntryProto to the metadata tree. Returns true on success.
  bool AddDriveEntryProto(int sequence_id,
                          bool is_directory,
                          const std::string& parent_resource_id);

  DriveResourceMetadata resource_metadata_;

 private:
  // Creates the following files/directories
  // drive/dir1/
  // drive/dir2/
  // drive/dir1/dir3/
  // drive/dir1/file4
  // drive/dir1/file5
  // drive/dir2/file6
  // drive/dir2/file7
  // drive/dir2/file8
  // drive/dir1/dir3/file9
  // drive/dir1/dir3/file10
  void Init();

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

DriveResourceMetadataTest::DriveResourceMetadataTest()
    : resource_metadata_(kTestRootResourceId),
      ui_thread_(content::BrowserThread::UI, &message_loop_) {
  Init();
}

void DriveResourceMetadataTest::Init() {
  int sequence_id = 1;
  ASSERT_TRUE(AddDriveEntryProto(
      sequence_id++, true, kTestRootResourceId));
  ASSERT_TRUE(AddDriveEntryProto(
      sequence_id++, true, kTestRootResourceId));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir1"));

  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir1"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir1"));

  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir2"));

  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir3"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, false, "resource_id:dir3"));
}

DriveEntryProto DriveResourceMetadataTest::CreateDriveEntryProto(
    int sequence_id,
    bool is_directory,
    const std::string& parent_resource_id) {
  DriveEntryProto entry_proto;
  const std::string sequence_id_str = base::IntToString(sequence_id);
  const std::string title = (is_directory ? "dir" : "file") +
                                sequence_id_str;
  const std::string resource_id = "resource_id:" + title;
  entry_proto.set_title(title);
  entry_proto.set_resource_id(resource_id);
  entry_proto.set_parent_resource_id(parent_resource_id);

  PlatformFileInfoProto* file_info = entry_proto.mutable_file_info();
  file_info->set_is_directory(is_directory);

  if (!is_directory) {
    DriveFileSpecificInfo* file_specific_info =
        entry_proto.mutable_file_specific_info();
    file_info->set_size(sequence_id * 1024);
    file_specific_info->set_file_md5(std::string("md5:") + title);
  }
  return entry_proto;
}

bool DriveResourceMetadataTest::AddDriveEntryProto(
    int sequence_id,
    bool is_directory,
    const std::string& parent_resource_id) {
  DriveEntryProto entry_proto = CreateDriveEntryProto(sequence_id,
                                                      is_directory,
                                                      parent_resource_id);
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;

  resource_metadata_.AddEntryToParent(
      entry_proto,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  return DRIVE_FILE_OK == error;
}

TEST_F(DriveResourceMetadataTest, VersionCheck) {
  // Set up the root directory.
  DriveRootDirectoryProto proto;
  DriveEntryProto* mutable_entry =
      proto.mutable_drive_directory()->mutable_drive_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_resource_id(kTestRootResourceId);
  mutable_entry->set_upload_url(kResumableCreateMediaUrl);
  mutable_entry->set_title("drive");

  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is empty.
  ASSERT_FALSE(resource_metadata.ParseFromString(serialized_proto));

  // Set an older version, and serialize.
  proto.set_version(kProtoVersion - 1);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is older.
  ASSERT_FALSE(resource_metadata.ParseFromString(serialized_proto));

  // Set the current version, and serialize.
  proto.set_version(kProtoVersion);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should succeed as the version matches the current number.
  ASSERT_TRUE(resource_metadata.ParseFromString(serialized_proto));

  // Set a newer version, and serialize.
  proto.set_version(kProtoVersion + 1);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is newer.
  ASSERT_FALSE(resource_metadata.ParseFromString(serialized_proto));
}

TEST_F(DriveResourceMetadataTest, LargestChangestamp) {
  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  int64 in_changestamp = 123456;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  resource_metadata.SetLargestChangestamp(
      in_changestamp,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                 &error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  int64 out_changestamp = 0;
  resource_metadata.GetLargestChangestamp(
      base::Bind(&CopyResultFromGetChangestampCallback,
                 &out_changestamp));
  google_apis::test_util::RunBlockingPoolTask();
  DCHECK_EQ(in_changestamp, out_changestamp);
}

TEST_F(DriveResourceMetadataTest, GetEntryByResourceId_RootDirectory) {
  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Look up the root directory by its resource ID.
  resource_metadata.GetEntryInfoByResourceId(
      kTestRootResourceId,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoByResourceId) {
  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_.GetEntryInfoByResourceId(
      "resource_id:file4",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file4"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file4", entry_proto->base_name());

  // Confirm that a non existing file is not found.
  error = DRIVE_FILE_ERROR_FAILED;
  entry_proto.reset();
  resource_metadata_.GetEntryInfoByResourceId(
      "file:non_existing",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoByPath) {
  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file4", entry_proto->base_name());

  // Confirm that a non existing file is not found.
  error = DRIVE_FILE_ERROR_FAILED;
  entry_proto.reset();
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/non_existing"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

TEST_F(DriveResourceMetadataTest, ReadDirectoryByPath) {
  // Confirm that an existing directory is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  resource_metadata_.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());

  // The order is not guaranteed so we should sort the base names.
  std::vector<std::string> base_names;
  for (size_t i = 0; i < 3; ++i)
    base_names.push_back(entries->at(i).base_name());
  std::sort(base_names.begin(), base_names.end());

  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Confirm that a non existing directory is not found.
  error = DRIVE_FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/non_existing"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entries.get());

  // Confirm that reading a file results in DRIVE_FILE_ERROR_NOT_A_DIRECTORY.
  error = DRIVE_FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_FALSE(entries.get());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoPairByPaths) {
  // Confirm that existing two files are found.
  scoped_ptr<EntryInfoPairResult> pair_result;
  resource_metadata_.GetEntryInfoPairByPaths(
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      FilePath::FromUTF8Unsafe("drive/dir1/file5"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoPairCallback,
                 &pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should be found.
  EXPECT_EQ(DRIVE_FILE_OK, pair_result->first.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file4"),
            pair_result->first.path);
  ASSERT_TRUE(pair_result->first.proto.get());
  EXPECT_EQ("file4", pair_result->first.proto->base_name());
  // The second entry should be found.
  EXPECT_EQ(DRIVE_FILE_OK, pair_result->second.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file5"),
            pair_result->second.path);
  ASSERT_TRUE(pair_result->second.proto.get());
  EXPECT_EQ("file5", pair_result->second.proto->base_name());

  // Confirm that the first non existent file is not found.
  pair_result.reset();
  resource_metadata_.GetEntryInfoPairByPaths(
      FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
      FilePath::FromUTF8Unsafe("drive/dir1/file5"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoPairCallback,
                 &pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should not be found.
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, pair_result->first.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
            pair_result->first.path);
  ASSERT_FALSE(pair_result->first.proto.get());
  // The second entry should not be found, because the first one failed.
  EXPECT_EQ(DRIVE_FILE_ERROR_FAILED, pair_result->second.error);
  EXPECT_EQ(FilePath(), pair_result->second.path);
  ASSERT_FALSE(pair_result->second.proto.get());

  // Confirm that the second non existent file is not found.
  pair_result.reset();
  resource_metadata_.GetEntryInfoPairByPaths(
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoPairCallback,
                 &pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should be found.
  EXPECT_EQ(DRIVE_FILE_OK, pair_result->first.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file4"),
            pair_result->first.path);
  ASSERT_TRUE(pair_result->first.proto.get());
  EXPECT_EQ("file4", pair_result->first.proto->base_name());
  // The second entry should not be found.
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, pair_result->second.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
            pair_result->second.path);
  ASSERT_FALSE(pair_result->second.proto.get());
}

TEST_F(DriveResourceMetadataTest, DBTest) {
  TestingProfile profile;
  scoped_refptr<base::SequencedWorkerPool> pool =
      content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      pool->GetSequencedTaskRunner(pool->GetSequenceToken());

  FilePath db_path(DriveCache::GetCacheRootPath(&profile).
      AppendASCII("meta").AppendASCII("resource_metadata.db"));
  // InitFromDB should fail with DRIVE_FILE_ERROR_NOT_FOUND since the db
  // doesn't exist.
  DriveFileError db_error;
  resource_metadata_.InitFromDB(
      db_path,
      blocking_task_runner,
      base::Bind(&CopyResultsFromInitFromDBCallback, &db_error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, db_error);

  // Create a file system and write it to disk.
  // We cannot call SaveToDB without first having called InitFromDB because
  // InitFrom initializes the db_path and blocking_task_runner needed by
  // SaveToDB.
  resource_metadata_.SaveToDB();
  google_apis::test_util::RunBlockingPoolTask();

  // InitFromDB should fail with DRIVE_FILE_ERROR_IN_USE.
  resource_metadata_.InitFromDB(
      db_path,
      blocking_task_runner,
      base::Bind(&CopyResultsFromInitFromDBCallback, &db_error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(DRIVE_FILE_ERROR_IN_USE, db_error);

  // InitFromDB should succeed.
  DriveResourceMetadata test_resource_metadata(kTestRootResourceId);
  test_resource_metadata.InitFromDB(
      db_path,
      blocking_task_runner,
      base::Bind(&CopyResultsFromInitFromDBCallback, &db_error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(DRIVE_FILE_OK, db_error);

  // Verify by checking for drive/dir2, which should have 3 children.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  test_resource_metadata.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());
}

TEST_F(DriveResourceMetadataTest, RemoveEntryFromParent) {
  // Make sure file9 is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  const std::string file9_resource_id = "resource_id:file9";
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_.GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());

  // Remove file9 using RemoveEntryFromParent.
  resource_metadata_.RemoveEntryFromParent(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);

  // file9 should no longer exist.
  resource_metadata_.GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Look for dir3.
  const std::string dir3_resource_id = "resource_id:dir3";
  resource_metadata_.GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir3", entry_proto->base_name());

  // Remove dir3 using RemoveEntryFromParent.
  resource_metadata_.RemoveEntryFromParent(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1"), drive_file_path);

  // dir3 should no longer exist.
  resource_metadata_.GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Remove unknown resource_id using RemoveEntryFromParent.
  resource_metadata_.RemoveEntryFromParent(
      "foo",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  // Try removing root. This should fail.
  resource_metadata_.RemoveEntryFromParent(
      kTestRootResourceId,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_ACCESS_DENIED, error);
}

TEST_F(DriveResourceMetadataTest, MoveEntryToDirectory) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Move file8 to drive/dir1.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file8"), drive_file_path);

  // Look up the entry by its resource id and make sure it really moved.
  resource_metadata_.GetEntryInfoByResourceId(
      "resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file8"), drive_file_path);

  // Move non-existent file to drive/dir1. This should fail.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Move existing file to non-existent directory. This should fail.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      FilePath::FromUTF8Unsafe("drive/dir4"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Move existing file to existing file (non-directory). This should fail.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Move the file to root.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/file8"), drive_file_path);

  // Move the file from root.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/file8"),
      FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file8"), drive_file_path);

  // Make sure file is still ok.
  resource_metadata_.GetEntryInfoByResourceId(
      "resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file8"), drive_file_path);
}

TEST_F(DriveResourceMetadataTest, RenameEntry) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Rename file8 to file11.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      "file11",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file11"), drive_file_path);

  // Lookup the file by resource id to make sure the file actually got renamed.
  resource_metadata_.GetEntryInfoByResourceId(
      "resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file11"), drive_file_path);

  // Rename to file7 to force a duplicate name.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file11"),
      "file7",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file7 (2)"), drive_file_path);

  // Rename to same name. This should fail.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file7 (2)"),
      "file7 (2)",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_EXISTS, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Rename non-existent.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file11"),
      "file11",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FilePath(), drive_file_path);
}

TEST_F(DriveResourceMetadataTest, RefreshEntry) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Get file9.
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());
  ASSERT_TRUE(!entry_proto->file_info().is_directory());
  EXPECT_EQ("md5:file9", entry_proto->file_specific_info().file_md5());

  // Rename it and change the file size.
  DriveEntryProto file_entry_proto(*entry_proto);
  const std::string updated_md5("md5:updated");
  file_entry_proto.mutable_file_specific_info()->set_file_md5(updated_md5);
  file_entry_proto.set_title("file100");
  entry_proto.reset();
  resource_metadata_.RefreshEntry(
      file_entry_proto,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3/file100"),
            drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file100", entry_proto->base_name());
  ASSERT_TRUE(!entry_proto->file_info().is_directory());
  EXPECT_EQ(updated_md5, entry_proto->file_specific_info().file_md5());

  // Make sure we get the same thing from GetEntryInfoByPath.
  entry_proto.reset();
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/dir3/file100"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file100", entry_proto->base_name());
  ASSERT_TRUE(!entry_proto->file_info().is_directory());
  EXPECT_EQ(updated_md5, entry_proto->file_specific_info().file_md5());

  // Get dir2.
  entry_proto.reset();
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir2", entry_proto->base_name());
  ASSERT_TRUE(entry_proto->file_info().is_directory());

  // Change the name to dir100 and change the parent to drive/dir1/dir3.
  DriveEntryProto dir_entry_proto(*entry_proto);
  dir_entry_proto.set_title("dir100");
  dir_entry_proto.set_parent_resource_id("resource_id:dir3");
  entry_proto.reset();
  resource_metadata_.RefreshEntry(
      dir_entry_proto,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3/dir100"),
            drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir100", entry_proto->base_name());
  EXPECT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ("resource_id:dir2", entry_proto->resource_id());

  // Make sure the children have moved over. Test file6.
  entry_proto.reset();
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/dir3/dir100/file6"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file6", entry_proto->base_name());

  // Make sure dir2 no longer exists.
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

// Test the special logic for RefreshEntry of root.
TEST_F(DriveResourceMetadataTest, RefreshEntry_Root) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Get root.
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
  ASSERT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(kTestRootResourceId, entry_proto->resource_id());
  EXPECT_TRUE(entry_proto->upload_url().empty());

  // Set upload url and call RefreshEntry on root.
  DriveEntryProto dir_entry_proto(*entry_proto);
  dir_entry_proto.set_upload_url("http://root.upload.url/");
  entry_proto.reset();
  resource_metadata_.RefreshEntry(
      dir_entry_proto,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
  EXPECT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(kTestRootResourceId, entry_proto->resource_id());
  EXPECT_EQ("http://root.upload.url/", entry_proto->upload_url());

  // Make sure the children have moved over. Test file9.
  entry_proto.reset();
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());
}

TEST_F(DriveResourceMetadataTest, AddEntryToParent) {
  int sequence_id = 100;
  DriveEntryProto file_entry_proto = CreateDriveEntryProto(
      sequence_id++, false, "resource_id:dir3");

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;

  // Add to dir3.
  resource_metadata_.AddEntryToParent(
      file_entry_proto,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3/file100"),
            drive_file_path);

  // Adds to root when parent resource id is not specified.
  DriveEntryProto file_entry_proto2 = CreateDriveEntryProto(
      sequence_id++, false, "");

  resource_metadata_.AddEntryToParent(
      file_entry_proto2,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/file101"), drive_file_path);

  // Add a directory.
  DriveEntryProto dir_entry_proto = CreateDriveEntryProto(
      sequence_id++, true, "resource_id:dir1");

  resource_metadata_.AddEntryToParent(
      dir_entry_proto,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir102"), drive_file_path);

  // Add to an invalid parent.
  DriveEntryProto file_entry_proto3 = CreateDriveEntryProto(
      sequence_id++, false, "resource_id:invalid");

  resource_metadata_.AddEntryToParent(
      file_entry_proto3,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
}

TEST_F(DriveResourceMetadataTest, GetChildDirectories) {
  std::set<FilePath> child_directories;

  // file9: not a directory, so no children.
  resource_metadata_.GetChildDirectories("resource_id:file9",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(child_directories.empty());

  // dir2: no child directories.
  resource_metadata_.GetChildDirectories("resource_id:dir2",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(child_directories.empty());

  // dir1: dir3 is the only child
  resource_metadata_.GetChildDirectories("resource_id:dir1",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(1u, child_directories.size());
  EXPECT_EQ(1u, child_directories.count(
      FilePath::FromUTF8Unsafe("drive/dir1/dir3")));

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
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir102"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir105"));
  ASSERT_TRUE(AddDriveEntryProto(sequence_id++, true, "resource_id:dir106"));

  resource_metadata_.GetChildDirectories("resource_id:dir2",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(8u, child_directories.size());
  EXPECT_EQ(1u, child_directories.count(FilePath::FromUTF8Unsafe(
      "drive/dir2/dir101")));
  EXPECT_EQ(1u, child_directories.count(FilePath::FromUTF8Unsafe(
      "drive/dir2/dir101/dir104")));
  EXPECT_EQ(1u, child_directories.count(FilePath::FromUTF8Unsafe(
      "drive/dir2/dir101/dir102/dir105/dir106/dir107")));
}

TEST_F(DriveResourceMetadataTest, RemoveAll) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;

  // root has children.
  resource_metadata_.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_FALSE(entries->empty());

  // remove all children.
  resource_metadata_.RemoveAll(base::Bind(&base::DoNothing));
  google_apis::test_util::RunBlockingPoolTask();

  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // root should continue to exist.
  resource_metadata_.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
  ASSERT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(kTestRootResourceId, entry_proto->resource_id());

  // root should have no children.
  resource_metadata_.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  EXPECT_TRUE(entries->empty());
}

}  // namespace drive
