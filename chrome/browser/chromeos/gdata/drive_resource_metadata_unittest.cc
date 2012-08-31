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
#include "chrome/browser/chromeos/gdata/gdata_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace {

// See drive.proto for the difference between the two URLs.
const char kResumableEditMediaUrl[] = "http://resumable-edit-media/";
const char kResumableCreateMediaUrl[] = "http://resumable-create-media/";

// Add a directory to |parent| and return that directory. The name and
// resource_id are determined by the incrementing counter |sequence_id|.
DriveDirectory* AddDirectory(DriveDirectory* parent,
                             DriveResourceMetadata* resource_metadata,
                             int sequence_id) {
  scoped_ptr<DriveDirectory> dir = resource_metadata->CreateDriveDirectory();
  const std::string dir_name = "dir" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("dir_resource_id:") +
                                  dir_name;
  dir->set_title(dir_name);
  dir->set_resource_id(resource_id);
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath moved_file_path;
  resource_metadata->MoveEntryToDirectory(
      parent->GetFilePath(),
      dir.get(),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error,
                 &moved_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(parent->GetFilePath().AppendASCII(dir_name), moved_file_path);
  return dir.release();
}

// Add a file to |parent| and return that file. The name and
// resource_id are determined by the incrementing counter |sequence_id|.
DriveFile* AddFile(DriveDirectory* parent,
                   DriveResourceMetadata* resource_metadata,
                   int sequence_id) {
  scoped_ptr<DriveFile> file = resource_metadata->CreateDriveFile();
  const std::string title = "file" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("file_resource_id:") +
                                  title;
  file->set_title(title);
  file->set_resource_id(resource_id);
  file->set_file_md5(std::string("file_md5:") + title);
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath moved_file_path;
  resource_metadata->MoveEntryToDirectory(
      parent->GetFilePath(),
      file.get(),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error,
                 &moved_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(parent->GetFilePath().AppendASCII(title), moved_file_path);
  return file.release();
}

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
void InitDirectoryService(DriveResourceMetadata* resource_metadata) {
  int sequence_id = 1;
  DriveDirectory* dir1 = AddDirectory(resource_metadata->root(),
      resource_metadata, sequence_id++);
  DriveDirectory* dir2 = AddDirectory(resource_metadata->root(),
      resource_metadata, sequence_id++);
  DriveDirectory* dir3 = AddDirectory(dir1, resource_metadata, sequence_id++);

  AddFile(dir1, resource_metadata, sequence_id++);
  AddFile(dir1, resource_metadata, sequence_id++);

  AddFile(dir2, resource_metadata, sequence_id++);
  AddFile(dir2, resource_metadata, sequence_id++);
  AddFile(dir2, resource_metadata, sequence_id++);

  AddFile(dir3, resource_metadata, sequence_id++);
  AddFile(dir3, resource_metadata, sequence_id++);
}

// Find directory by path.
DriveDirectory* FindDirectory(DriveResourceMetadata* resource_metadata,
                              const char* path) {
  return resource_metadata->FindEntryByPathSync(
      FilePath(path))->AsDriveDirectory();
}

// Find file by path.
DriveFile* FindFile(DriveResourceMetadata* resource_metadata,
                    const char* path) {
  return resource_metadata->FindEntryByPathSync(FilePath(path))->AsDriveFile();
}

// Verify that the recreated directory service matches what we created in
// InitDirectoryService.
void VerifyDirectoryService(DriveResourceMetadata* resource_metadata) {
  ASSERT_TRUE(resource_metadata->root());

  DriveDirectory* dir1 = FindDirectory(resource_metadata, "drive/dir1");
  ASSERT_TRUE(dir1);
  DriveDirectory* dir2 = FindDirectory(resource_metadata, "drive/dir2");
  ASSERT_TRUE(dir2);
  DriveDirectory* dir3 = FindDirectory(resource_metadata, "drive/dir1/dir3");
  ASSERT_TRUE(dir3);

  DriveFile* file4 = FindFile(resource_metadata, "drive/dir1/file4");
  ASSERT_TRUE(file4);
  EXPECT_EQ(file4->parent(), dir1);

  DriveFile* file5 = FindFile(resource_metadata, "drive/dir1/file5");
  ASSERT_TRUE(file5);
  EXPECT_EQ(file5->parent(), dir1);

  DriveFile* file6 = FindFile(resource_metadata, "drive/dir2/file6");
  ASSERT_TRUE(file6);
  EXPECT_EQ(file6->parent(), dir2);

  DriveFile* file7 = FindFile(resource_metadata, "drive/dir2/file7");
  ASSERT_TRUE(file7);
  EXPECT_EQ(file7->parent(), dir2);

  DriveFile* file8 = FindFile(resource_metadata, "drive/dir2/file8");
  ASSERT_TRUE(file8);
  EXPECT_EQ(file8->parent(), dir2);

  DriveFile* file9 = FindFile(resource_metadata, "drive/dir1/dir3/file9");
  ASSERT_TRUE(file9);
  EXPECT_EQ(file9->parent(), dir3);

  DriveFile* file10 = FindFile(resource_metadata, "drive/dir1/dir3/file10");
  ASSERT_TRUE(file10);
  EXPECT_EQ(file10->parent(), dir3);

  EXPECT_EQ(dir1, resource_metadata->GetEntryByResourceId(
      "dir_resource_id:dir1"));
  EXPECT_EQ(dir2, resource_metadata->GetEntryByResourceId(
      "dir_resource_id:dir2"));
  EXPECT_EQ(dir3, resource_metadata->GetEntryByResourceId(
      "dir_resource_id:dir3"));
  EXPECT_EQ(file4, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file4"));
  EXPECT_EQ(file5, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file5"));
  EXPECT_EQ(file6, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file6"));
  EXPECT_EQ(file7, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file7"));
  EXPECT_EQ(file8, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file8"));
  EXPECT_EQ(file9, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file9"));
  EXPECT_EQ(file10, resource_metadata->GetEntryByResourceId(
      "file_resource_id:file10"));
}

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

TEST(DriveResourceMetadataTest, VersionCheck) {
  // Set up the root directory.
  DriveRootDirectoryProto proto;
  DriveEntryProto* mutable_entry =
      proto.mutable_gdata_directory()->mutable_gdata_entry();
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

TEST(DriveResourceMetadataTest, GetEntryByResourceId_RootDirectory) {
  DriveResourceMetadata resource_metadata;
  // Look up the root directory by its resource ID.
  DriveEntry* entry = resource_metadata.GetEntryByResourceId(
      kDriveRootDirectoryResourceId);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kDriveRootDirectoryResourceId, entry->resource_id());
}

TEST(DriveResourceMetadataTest, GetEntryInfoByResourceId) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  DriveResourceMetadata resource_metadata;
  InitDirectoryService(&resource_metadata);

  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata.GetEntryInfoByResourceId(
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
  resource_metadata.GetEntryInfoByResourceId(
      "file:non_existing",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

TEST(DriveResourceMetadataTest, GetEntryInfoByPath) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  DriveResourceMetadata resource_metadata;
  InitDirectoryService(&resource_metadata);

  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata.GetEntryInfoByPath(
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
  resource_metadata.GetEntryInfoByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/non_existing"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

TEST(DriveResourceMetadataTest, ReadDirectoryByPath) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  DriveResourceMetadata resource_metadata;
  InitDirectoryService(&resource_metadata);

  // Confirm that an existing directory is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  resource_metadata.ReadDirectoryByPath(
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
  resource_metadata.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/non_existing"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entries.get());

  // Confirm that reading a file results in DRIVE_FILE_ERROR_NOT_A_DIRECTORY.
  error = DRIVE_FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata.ReadDirectoryByPath(
      FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_FALSE(entries.get());
}

TEST(DriveResourceMetadataTest, GetEntryInfoPairByPaths) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  DriveResourceMetadata resource_metadata;
  InitDirectoryService(&resource_metadata);

  // Confirm that existing two files are found.
  scoped_ptr<EntryInfoPairResult> pair_result;
  resource_metadata.GetEntryInfoPairByPaths(
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
  resource_metadata.GetEntryInfoPairByPaths(
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
  resource_metadata.GetEntryInfoPairByPaths(
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

TEST(DriveResourceMetadataTest, DBTest) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);

  scoped_ptr<TestingProfile> profile(new TestingProfile);
  scoped_refptr<base::SequencedWorkerPool> pool =
      content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      pool->GetSequencedTaskRunner(pool->GetSequenceToken());

  DriveResourceMetadata resource_metadata;
  FilePath db_path(DriveCache::GetCacheRootPath(profile.get()).
      AppendASCII("meta").AppendASCII("resource_metadata.db"));
  // InitFromDB should fail with DRIVE_FILE_ERROR_NOT_FOUND since the db
  // doesn't exist.
  resource_metadata.InitFromDB(db_path, blocking_task_runner,
      base::Bind(&InitFromDBCallback, DRIVE_FILE_ERROR_NOT_FOUND));
  test_util::RunBlockingPoolTask();
  InitDirectoryService(&resource_metadata);

  // Write the filesystem to db.
  resource_metadata.SaveToDB();
  test_util::RunBlockingPoolTask();

  DriveResourceMetadata resource_metadata2;
  // InitFromDB should succeed with DRIVE_FILE_OK as the db now exists.
  resource_metadata2.InitFromDB(db_path, blocking_task_runner,
      base::Bind(&InitFromDBCallback, DRIVE_FILE_OK));
  test_util::RunBlockingPoolTask();

  VerifyDirectoryService(&resource_metadata2);
}

TEST(DriveResourceMetadataTest, RemoveEntryFromParent) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  DriveResourceMetadata resource_metadata;
  InitDirectoryService(&resource_metadata);

  // Make sure file9 is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath drive_file_path;
  const std::string file9_resource_id = "file_resource_id:file9";
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata.GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());

  // Remove file9 using RemoveEntryFromParent.
  resource_metadata.RemoveEntryFromParent(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);

  // file9 should no longer exist.
  resource_metadata.GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Look for dir3.
  const std::string dir3_resource_id = "dir_resource_id:dir3";
  resource_metadata.GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir3", entry_proto->base_name());

  // Remove dir3 using RemoveEntryFromParent.
  resource_metadata.RemoveEntryFromParent(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/dir1"), drive_file_path);

  // dir3 should no longer exist.
  resource_metadata.GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Remove unknown resource_id using RemoveEntryFromParent.
  resource_metadata.RemoveEntryFromParent(
      "foo",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  // Try removing root. This should fail.
  resource_metadata.RemoveEntryFromParent(
      kDriveRootDirectoryResourceId,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_ACCESS_DENIED, error);
}

}  // namespace gdata
