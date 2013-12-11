// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/common/fileapi/file_system_util.h"

#define FPL FILE_PATH_LITERAL

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kV0ServiceName[] = "drive";

bool CreateV0SerializedSyncableFileSystemURL(
    const GURL& origin,
    const base::FilePath& path,
    std::string* serialized_url) {
  fileapi::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kV0ServiceName, fileapi::kFileSystemTypeSyncable,
      fileapi::FileSystemMountOption(), base::FilePath());
  fileapi::FileSystemURL url =
      fileapi::ExternalMountPoints::GetSystemInstance()->
          CreateExternalFileSystemURL(origin, kV0ServiceName, path);
  fileapi::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kV0ServiceName);

  if (!url.is_valid())
    return false;
  *serialized_url = fileapi::GetExternalFileSystemRootURIString(
        origin, kV0ServiceName) + url.path().AsUTF8Unsafe();
  return true;
}

}  // namespace

TEST(DriveMetadataDBMigrationUtilTest, ParseV0FormatFileSystemURL) {
  const GURL kOrigin("chrome-extension://example");
  const base::FilePath kFile(FPL("foo bar"));

  std::string serialized_url;
  ASSERT_TRUE(CreateV0SerializedSyncableFileSystemURL(
      kOrigin, kFile, &serialized_url));

  GURL origin;
  base::FilePath path;
  EXPECT_TRUE(ParseV0FormatFileSystemURL(GURL(serialized_url), &origin, &path));
  EXPECT_EQ(kOrigin, origin);
  EXPECT_EQ(kFile, path);
}

TEST(DriveMetadataDBMigrationUtilTest, AddWapiIdPrefix) {
  DriveMetadata_ResourceType type_file =
      DriveMetadata_ResourceType_RESOURCE_TYPE_FILE;
  DriveMetadata_ResourceType type_folder =
      DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER;

  EXPECT_EQ("file:xxx", AddWapiFilePrefix("xxx"));
  EXPECT_EQ("folder:yyy", AddWapiFolderPrefix("yyy"));
  EXPECT_EQ("file:xxx", AddWapiIdPrefix("xxx", type_file));
  EXPECT_EQ("folder:yyy", AddWapiIdPrefix("yyy", type_folder));

  EXPECT_EQ("", AddWapiFilePrefix(""));
  EXPECT_EQ("", AddWapiFolderPrefix(""));
  EXPECT_EQ("", AddWapiIdPrefix("", type_file));
  EXPECT_EQ("", AddWapiIdPrefix("", type_folder));
}

TEST(DriveMetadataDBMigrationUtilTest, RemoveWapiIdPrefix) {
  EXPECT_EQ("xxx", RemoveWapiIdPrefix("xxx"));
  EXPECT_EQ("yyy", RemoveWapiIdPrefix("file:yyy"));
  EXPECT_EQ("zzz", RemoveWapiIdPrefix("folder:zzz"));

  EXPECT_EQ("", RemoveWapiIdPrefix(""));
  EXPECT_EQ("foo:xxx", RemoveWapiIdPrefix("foo:xxx"));
}

TEST(DriveMetadataDBMigrationUtilTest, MigrationFromV0) {
  const char kDatabaseVersionKey[] = "VERSION";
  const char kChangeStampKey[] = "CHANGE_STAMP";
  const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
  const char kDriveMetadataKeyPrefix[] = "METADATA: ";
  const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
  const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";

  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const std::string kResourceId1("folder:hoge");
  const std::string kResourceId2("folder:fuga");
  const std::string kFileResourceId("file:piyo");
  const base::FilePath kFile(FPL("foo bar"));
  const std::string kFileMD5("file_md5");

  base::ScopedTempDir base_dir;
  ASSERT_TRUE(base_dir.CreateUniqueTempDir());

  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.
  leveldb::DB* db_ptr = NULL;
  std::string db_dir = fileapi::FilePathToString(
      base_dir.path().Append(DriveMetadataStore::kDatabaseName));
  leveldb::Status status = leveldb::DB::Open(options, db_dir, &db_ptr);

  scoped_ptr<leveldb::DB> db(db_ptr);
  ASSERT_TRUE(status.ok());

  // Setup the database with the schema version 0.
  leveldb::WriteBatch batch;
  batch.Put(kChangeStampKey, "1");
  batch.Put(kSyncRootDirectoryKey, kSyncRootResourceId);

  // Setup drive metadata.
  DriveMetadata drive_metadata;
  drive_metadata.set_resource_id(kFileResourceId);
  drive_metadata.set_md5_checksum(kFileMD5);
  drive_metadata.set_conflicted(false);
  drive_metadata.set_to_be_fetched(false);

  std::string serialized_url;
  ASSERT_TRUE(CreateV0SerializedSyncableFileSystemURL(
      kOrigin1, kFile, &serialized_url));
  std::string metadata_string;
  drive_metadata.SerializeToString(&metadata_string);
  batch.Put(kDriveMetadataKeyPrefix + serialized_url, metadata_string);

  // Setup batch sync origin and incremental sync origin.
  batch.Put(kDriveBatchSyncOriginKeyPrefix + kOrigin1.spec(), kResourceId1);
  batch.Put(kDriveIncrementalSyncOriginKeyPrefix + kOrigin2.spec(),
            kResourceId2);

  status = db->Write(leveldb::WriteOptions(), &batch);
  EXPECT_EQ(SYNC_STATUS_OK, LevelDBStatusToSyncStatusCode(status));

  // Migrate the database.
  MigrateDatabaseFromV0ToV1(db.get());

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));

  // Verify DB schema version.
  int64 database_version = 0;
  itr->Seek(kDatabaseVersionKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_TRUE(base::StringToInt64(itr->value().ToString(), &database_version));
  EXPECT_EQ(1, database_version);

  // Verify the largest changestamp.
  int64 changestamp = 0;
  itr->Seek(kChangeStampKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_TRUE(base::StringToInt64(itr->value().ToString(), &changestamp));
  EXPECT_EQ(1, changestamp);

  // Verify the sync root directory.
  itr->Seek(kSyncRootDirectoryKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(kSyncRootResourceId, itr->value().ToString());

  // Verify the metadata.
  itr->Seek(kDriveMetadataKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  DriveMetadata metadata;
  EXPECT_TRUE(metadata.ParseFromString(itr->value().ToString()));
  EXPECT_EQ(kFileResourceId, metadata.resource_id());
  EXPECT_EQ(kFileMD5, metadata.md5_checksum());
  EXPECT_FALSE(metadata.conflicted());
  EXPECT_FALSE(metadata.to_be_fetched());

  // Verify the batch sync origin.
  itr->Seek(kDriveBatchSyncOriginKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(kResourceId1, itr->value().ToString());

  // Verify the incremental sync origin.
  itr->Seek(kDriveIncrementalSyncOriginKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(kResourceId2, itr->value().ToString());
}

TEST(DriveMetadataDBMigrationUtilTest, MigrationFromV1) {
  const char kDatabaseVersionKey[] = "VERSION";
  const char kChangeStampKey[] = "CHANGE_STAMP";
  const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
  const char kDriveMetadataKeyPrefix[] = "METADATA: ";
  const char kMetadataKeySeparator = ' ';
  const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
  const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
  const char kDriveDisabledOriginKeyPrefix[] = "DISABLED_ORIGIN: ";

  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");

  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const std::string kResourceId1("folder:hoge");
  const std::string kResourceId2("folder:fuga");
  const std::string kResourceId3("folder:hogera");
  const std::string kFileResourceId("file:piyo");
  const base::FilePath kFile(FPL("foo bar"));
  const std::string kFileMD5("file_md5");

  RegisterSyncableFileSystem();

  base::ScopedTempDir base_dir;
  ASSERT_TRUE(base_dir.CreateUniqueTempDir());

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db_ptr = NULL;
  std::string db_dir = fileapi::FilePathToString(
      base_dir.path().Append(DriveMetadataStore::kDatabaseName));
  leveldb::Status status = leveldb::DB::Open(options, db_dir, &db_ptr);

  scoped_ptr<leveldb::DB> db(db_ptr);
  ASSERT_TRUE(status.ok());

  // Setup the database with the schema version 1.
  leveldb::WriteBatch batch;
  batch.Put(kDatabaseVersionKey, "1");
  batch.Put(kChangeStampKey, "1");
  batch.Put(kSyncRootDirectoryKey, kSyncRootResourceId);

  fileapi::FileSystemURL url = CreateSyncableFileSystemURL(kOrigin1, kFile);

  // Setup drive metadata.
  DriveMetadata drive_metadata;
  drive_metadata.set_resource_id(kFileResourceId);
  drive_metadata.set_md5_checksum(kFileMD5);
  drive_metadata.set_conflicted(false);
  drive_metadata.set_to_be_fetched(false);
  std::string metadata_string;
  drive_metadata.SerializeToString(&metadata_string);
  std::string metadata_key = kDriveMetadataKeyPrefix + kOrigin1.spec() +
                             kMetadataKeySeparator + url.path().AsUTF8Unsafe();
  batch.Put(metadata_key, metadata_string);

  // Setup origins.
  batch.Put(kDriveBatchSyncOriginKeyPrefix + kOrigin1.spec(), kResourceId1);
  batch.Put(kDriveIncrementalSyncOriginKeyPrefix + kOrigin2.spec(),
            kResourceId2);
  batch.Put(kDriveDisabledOriginKeyPrefix + kOrigin3.spec(), kResourceId3);

  status = db->Write(leveldb::WriteOptions(), &batch);
  EXPECT_EQ(SYNC_STATUS_OK, LevelDBStatusToSyncStatusCode(status));

  RevokeSyncableFileSystem();

  // Migrate the database.
  MigrateDatabaseFromV1ToV2(db.get());

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));

  // Verify DB schema version.
  int64 database_version = 0;
  itr->Seek(kDatabaseVersionKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_TRUE(base::StringToInt64(itr->value().ToString(), &database_version));
  EXPECT_EQ(2, database_version);

  // Verify the largest changestamp.
  int64 changestamp = 0;
  itr->Seek(kChangeStampKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_TRUE(base::StringToInt64(itr->value().ToString(), &changestamp));
  EXPECT_EQ(1, changestamp);

  // Verify the sync root directory.
  itr->Seek(kSyncRootDirectoryKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(RemoveWapiIdPrefix(kSyncRootResourceId), itr->value().ToString());

  // Verify the metadata.
  itr->Seek(kDriveMetadataKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  DriveMetadata metadata;
  EXPECT_TRUE(metadata.ParseFromString(itr->value().ToString()));
  EXPECT_EQ(RemoveWapiIdPrefix(kFileResourceId), metadata.resource_id());
  EXPECT_EQ(kFileMD5, metadata.md5_checksum());
  EXPECT_FALSE(metadata.conflicted());
  EXPECT_FALSE(metadata.to_be_fetched());

  // Verify the batch sync origin.
  itr->Seek(kDriveBatchSyncOriginKeyPrefix);
  EXPECT_FALSE(StartsWithASCII(kDriveBatchSyncOriginKeyPrefix,
                               itr->key().ToString(), true));

  // Verify the incremental sync origin.
  itr->Seek(kDriveIncrementalSyncOriginKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(RemoveWapiIdPrefix(kResourceId2), itr->value().ToString());

  // Verify the disabled origin.
  itr->Seek(kDriveDisabledOriginKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(RemoveWapiIdPrefix(kResourceId3), itr->value().ToString());
}

}  // namespace drive_backend
}  // namespace sync_file_system
