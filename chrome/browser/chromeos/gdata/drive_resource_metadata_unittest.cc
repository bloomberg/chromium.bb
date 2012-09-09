// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_resource_metadata.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_files.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace {

// See drive.proto for the difference between the two URLs.
const char kResumableEditMediaUrl[] = "http://resumable-edit-media/";
const char kResumableCreateMediaUrl[] = "http://resumable-create-media/";

// Callback for DriveResourceMetadata::InitFromDB.
void InitFromDBCallback(DriveFileError expected_error,
                        DriveFileError actual_error) {
  EXPECT_EQ(expected_error, actual_error);
}

// Callback for DriveResourceMetadata::ReadDirectoryByPath.
void ReadDirectoryByPathCallback(
    scoped_ptr<DriveEntryProtoVector>* result,
    DriveFileError error,
    scoped_ptr<DriveEntryProtoVector> entries) {
  EXPECT_EQ(DRIVE_FILE_OK, error);
  *result = entries.Pass();
}

}  // namespace

class DriveResourceMetadataTest : public testing::Test {
 public:
  DriveResourceMetadataTest();

 protected:
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

  // Add a directory to |parent| and return that directory. The name and
  // resource_id are determined by the incrementing counter |sequence_id|.
  DriveDirectory* AddDirectory(DriveDirectory* parent, int sequence_id);

  // Add a file to |parent| and return that file. The name and
  // resource_id are determined by the incrementing counter |sequence_id|.
  DriveFile* AddFile(DriveDirectory* parent, int sequence_id);

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

DriveResourceMetadataTest::DriveResourceMetadataTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  Init();
}

void DriveResourceMetadataTest::Init() {
  int sequence_id = 1;
  DriveDirectory* dir1 = AddDirectory(resource_metadata_.root(), sequence_id++);
  DriveDirectory* dir2 = AddDirectory(resource_metadata_.root(), sequence_id++);
  DriveDirectory* dir3 = AddDirectory(dir1, sequence_id++);

  AddFile(dir1, sequence_id++);
  AddFile(dir1, sequence_id++);

  AddFile(dir2, sequence_id++);
  AddFile(dir2, sequence_id++);
  AddFile(dir2, sequence_id++);

  AddFile(dir3, sequence_id++);
  AddFile(dir3, sequence_id++);
}

DriveDirectory* DriveResourceMetadataTest::AddDirectory(DriveDirectory* parent,
                                                        int sequence_id) {
  scoped_ptr<DriveDirectory> dir = resource_metadata_.CreateDriveDirectory();
  const std::string dir_name = "dir" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("dir_resource_id:") + dir_name;
  dir->set_title(dir_name);
  dir->set_resource_id(resource_id);

  parent->AddEntry(dir.get());

  return dir.release();
}

DriveFile* DriveResourceMetadataTest::AddFile(DriveDirectory* parent,
                                              int sequence_id) {
  scoped_ptr<DriveFile> file = resource_metadata_.CreateDriveFile();
  const std::string title = "file" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("file_resource_id:") + title;
  file->set_title(title);
  file->set_resource_id(resource_id);
  file->set_file_md5(std::string("file_md5:") + title);

  parent->AddEntry(file.get());

  return file.release();
}

TEST_F(DriveResourceMetadataTest, VersionCheck) {
  // Set up the root directory.
  DriveRootDirectoryProto proto;
  DriveEntryProto* mutable_entry =
      proto.mutable_drive_directory()->mutable_drive_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_resource_id(kDriveRootDirectoryResourceId);
  mutable_entry->set_upload_url(kResumableCreateMediaUrl);
  mutable_entry->set_title("drive");

  DriveResourceMetadata resource_metadata;

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is emtpy.
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

TEST_F(DriveResourceMetadataTest, GetEntryByResourceId_RootDirectory) {
  DriveResourceMetadata resource_metadata;
  // Look up the root directory by its resource ID.
  DriveEntry* entry = resource_metadata.GetEntryByResourceId(
      kDriveRootDirectoryResourceId);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kDriveRootDirectoryResourceId, entry->resource_id());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoByResourceId) {
  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_.GetEntryInfoByResourceId(
      "file_resource_id:file4",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entries.get());

  // Confirm that reading a file results in DRIVE_FILE_ERROR_NOT_A_DIRECTORY.
  error = DRIVE_FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
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
  resource_metadata_.InitFromDB(db_path, blocking_task_runner,
      base::Bind(&InitFromDBCallback, DRIVE_FILE_ERROR_NOT_FOUND));
  test_util::RunBlockingPoolTask();

  // Create a file system and write it to disk.
  // We cannot call SaveToDB without first having called InitFromDB because
  // InitFrom initializes the db_path and blocking_task_runner needed by
  // SaveToDB.
  resource_metadata_.SaveToDB();
  test_util::RunBlockingPoolTask();

  // InitFromDB should fail with DRIVE_FILE_ERROR_IN_USE.
  resource_metadata_.InitFromDB(db_path, blocking_task_runner,
      base::Bind(&InitFromDBCallback, DRIVE_FILE_ERROR_IN_USE));
  test_util::RunBlockingPoolTask();

  // InitFromDB should succeed.
  DriveResourceMetadata test_resource_metadata;
  test_resource_metadata.InitFromDB(db_path, blocking_task_runner,
      base::Bind(&InitFromDBCallback, DRIVE_FILE_OK));
  test_util::RunBlockingPoolTask();

  // Verify by checking for drive/dir2, which should have 3 children.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  test_resource_metadata.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());
}

TEST_F(DriveResourceMetadataTest, RemoveEntryFromParent) {
  // Make sure file9 is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  const std::string file9_resource_id = "file_resource_id:file9";
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_.GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());

  // Remove file9 using RemoveEntryFromParent.
  resource_metadata_.RemoveEntryFromParent(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);

  // file9 should no longer exist.
  resource_metadata_.GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Look for dir3.
  const std::string dir3_resource_id = "dir_resource_id:dir3";
  resource_metadata_.GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir3", entry_proto->base_name());

  // Remove dir3 using RemoveEntryFromParent.
  resource_metadata_.RemoveEntryFromParent(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1"), drive_file_path);

  // dir3 should no longer exist.
  resource_metadata_.GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Remove unknown resource_id using RemoveEntryFromParent.
  resource_metadata_.RemoveEntryFromParent(
      "foo",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  // Try removing root. This should fail.
  resource_metadata_.RemoveEntryFromParent(
      kDriveRootDirectoryResourceId,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file8"), drive_file_path);

  // Look up the entry by its resource id and make sure it really moved.
  resource_metadata_.GetEntryInfoByResourceId(
      "file_resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/file8"), drive_file_path);

  // Move non-existent file to drive/dir1. This should fail.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Move existing file to non-existent directory. This should fail.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      FilePath::FromUTF8Unsafe("drive/dir4"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Move existing file to existing file (non-directory). This should fail.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Move the file to root.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/file8"), drive_file_path);

  // Move the file from root.
  resource_metadata_.MoveEntryToDirectory(
      FilePath::FromUTF8Unsafe("drive/file8"),
      FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file8"), drive_file_path);

  // Make sure file is still ok.
  resource_metadata_.GetEntryInfoByResourceId(
      "file_resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
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
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file11"), drive_file_path);

  // Lookup the file by resource id to make sure the file actually got renamed.
  resource_metadata_.GetEntryInfoByResourceId(
      "file_resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file11"), drive_file_path);

  // Rename to file7 to force a duplicate name.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file11"),
      "file7",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir2/file7 (2)"), drive_file_path);

  // Rename to same name. This should fail.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file7 (2)"),
      "file7 (2)",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_EXISTS, error);
  EXPECT_EQ(FilePath(), drive_file_path);

  // Rename non-existent.
  resource_metadata_.RenameEntry(
      FilePath::FromUTF8Unsafe("drive/dir2/file11"),
      "file11",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FilePath(), drive_file_path);
}

}  // namespace gdata
