// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive/metadata_db_migration_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/common/fileapi/file_system_util.h"

#define FPL FILE_PATH_LITERAL

namespace sync_file_system {
namespace drive {

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

TEST(DriveMetadataDBMigrationUtilTest, MigrationFromV1) {
  const char kDatabaseVersionKey[] = "VERSION";
  const char kChangeStampKey[] = "CHANGE_STAMP";
  const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
  const char kDriveMetadataKeyPrefix[] = "METADATA: ";
  const char kMetadataKeySeparator = ' ';
  const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
  const char kDriveDisabledOriginKeyPrefix[] = "DISABLED_ORIGIN: ";

  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");

  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const std::string kResourceId1("folder:hoge");
  const std::string kResourceId2("folder:fuga");
  const std::string kFileResourceId("file:piyo");
  const base::FilePath kFile(FPL("foo bar"));
  const std::string kFileMD5("file_md5");

  const char kV1ServiceName[] = "drive";
  ASSERT_TRUE(RegisterSyncableFileSystem(kV1ServiceName));

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

  // Setup the database with the scheme version 1.
  leveldb::WriteBatch batch;
  batch.Put(kDatabaseVersionKey, "1");
  batch.Put(kChangeStampKey, "1");
  batch.Put(kSyncRootDirectoryKey, kSyncRootResourceId);

  fileapi::FileSystemURL url =
      CreateSyncableFileSystemURL(kOrigin1, kV1ServiceName, kFile);

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

  // Setup incremental sync origin and disabled origin.
  batch.Put(kDriveIncrementalSyncOriginKeyPrefix + kOrigin1.spec(),
            kResourceId1);
  batch.Put(kDriveDisabledOriginKeyPrefix + kOrigin2.spec(),
            kResourceId2);

  status = db->Write(leveldb::WriteOptions(), &batch);
  EXPECT_EQ(SYNC_STATUS_OK, LevelDBStatusToSyncStatusCode(status));
  EXPECT_TRUE(RevokeSyncableFileSystem(kV1ServiceName));

  // Migrate the database.
  drive::MigrateDatabaseFromV1ToV2(db.get());

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));

  // Verify DB scheme version.
  int64 database_version = 0;
  itr->Seek(kDatabaseVersionKey);
  EXPECT_TRUE(itr->Valid());
  EXPECT_TRUE(base::StringToInt64(itr->value().ToString(), &database_version));
  EXPECT_EQ(2, database_version);

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

  // Verify the incremental sync origin.
  itr->Seek(kDriveIncrementalSyncOriginKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(RemoveWapiIdPrefix(kResourceId1), itr->value().ToString());

  // Verify the disabled origin.
  itr->Seek(kDriveDisabledOriginKeyPrefix);
  EXPECT_TRUE(itr->Valid());
  EXPECT_EQ(RemoveWapiIdPrefix(kResourceId2), itr->value().ToString());
}

}  // namespace drive
}  // namespace sync_file_system
