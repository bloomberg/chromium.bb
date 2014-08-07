// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const base::FilePath::CharType kV0FormatPathPrefix[] =
    FILE_PATH_LITERAL("drive/");
const char kWapiFileIdPrefix[] = "file:";
const char kWapiFolderIdPrefix[] = "folder:";

}  // namespace

bool ParseV0FormatFileSystemURL(const GURL& url,
                                GURL* origin,
                                base::FilePath* path) {
  fileapi::FileSystemType mount_type;
  base::FilePath virtual_path;

  if (!fileapi::ParseFileSystemSchemeURL(
          url, origin, &mount_type, &virtual_path) ||
      mount_type != fileapi::kFileSystemTypeExternal) {
    NOTREACHED() << "Failed to parse filesystem scheme URL " << url.spec();
    return false;
  }

  base::FilePath::StringType prefix =
      base::FilePath(kV0FormatPathPrefix).NormalizePathSeparators().value();
  if (virtual_path.value().substr(0, prefix.size()) != prefix)
    return false;

  *path = base::FilePath(virtual_path.value().substr(prefix.size()));
  return true;
}

std::string AddWapiFilePrefix(const std::string& resource_id) {
  DCHECK(!StartsWithASCII(resource_id, kWapiFileIdPrefix, true));
  DCHECK(!StartsWithASCII(resource_id, kWapiFolderIdPrefix, true));

  if (resource_id.empty() ||
      StartsWithASCII(resource_id, kWapiFileIdPrefix, true) ||
      StartsWithASCII(resource_id, kWapiFolderIdPrefix, true))
    return resource_id;
  return kWapiFileIdPrefix + resource_id;
}

std::string AddWapiFolderPrefix(const std::string& resource_id) {
  DCHECK(!StartsWithASCII(resource_id, kWapiFileIdPrefix, true));
  DCHECK(!StartsWithASCII(resource_id, kWapiFolderIdPrefix, true));

  if (resource_id.empty() ||
      StartsWithASCII(resource_id, kWapiFileIdPrefix, true) ||
      StartsWithASCII(resource_id, kWapiFolderIdPrefix, true))
    return resource_id;
  return kWapiFolderIdPrefix + resource_id;
}

std::string AddWapiIdPrefix(const std::string& resource_id,
                            DriveMetadata_ResourceType type) {
  switch (type) {
    case DriveMetadata_ResourceType_RESOURCE_TYPE_FILE:
      return AddWapiFilePrefix(resource_id);
    case DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER:
      return AddWapiFolderPrefix(resource_id);
  }
  NOTREACHED();
  return resource_id;
}

std::string RemoveWapiIdPrefix(const std::string& resource_id) {
  std::string value;
  if (RemovePrefix(resource_id, kWapiFileIdPrefix, &value))
    return value;
  if (RemovePrefix(resource_id, kWapiFolderIdPrefix, &value))
    return value;
  return resource_id;
}

SyncStatusCode MigrateDatabaseFromV0ToV1(leveldb::DB* db) {
  // Version 0 database format:
  //   key: "CHANGE_STAMP"
  //   value: <Largest Changestamp>
  //
  //   key: "SYNC_ROOT_DIR"
  //   value: <Resource ID of the sync root directory>
  //
  //   key: "METADATA: " +
  //        <FileSystemURL serialized by SerializeSyncableFileSystemURL>
  //   value: <Serialized DriveMetadata>
  //
  //   key: "BSYNC_ORIGIN: " + <URL string of a batch sync origin>
  //   value: <Resource ID of the drive directory for the origin>
  //
  //   key: "ISYNC_ORIGIN: " + <URL string of a incremental sync origin>
  //   value: <Resource ID of the drive directory for the origin>
  //
  // Version 1 database format (changed keys/fields are marked with '*'):
  // * key: "VERSION" (new)
  // * value: 1
  //
  //   key: "CHANGE_STAMP"
  //   value: <Largest Changestamp>
  //
  //   key: "SYNC_ROOT_DIR"
  //   value: <Resource ID of the sync root directory>
  //
  // * key: "METADATA: " + <Origin and URL> (changed)
  // * value: <Serialized DriveMetadata>
  //
  //   key: "BSYNC_ORIGIN: " + <URL string of a batch sync origin>
  //   value: <Resource ID of the drive directory for the origin>
  //
  //   key: "ISYNC_ORIGIN: " + <URL string of a incremental sync origin>
  //   value: <Resource ID of the drive directory for the origin>
  //
  //   key: "DISABLED_ORIGIN: " + <URL string of a disabled origin>
  //   value: <Resource ID of the drive directory for the origin>

  const char kDatabaseVersionKey[] = "VERSION";
  const char kDriveMetadataKeyPrefix[] = "METADATA: ";
  const char kMetadataKeySeparator = ' ';

  leveldb::WriteBatch write_batch;
  write_batch.Put(kDatabaseVersionKey, "1");

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kDriveMetadataKeyPrefix); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveMetadataKeyPrefix, true))
      break;
    std::string serialized_url;
    RemovePrefix(key, kDriveMetadataKeyPrefix, &serialized_url);

    GURL origin;
    base::FilePath path;
    bool success = ParseV0FormatFileSystemURL(
        GURL(serialized_url), &origin, &path);
    DCHECK(success) << serialized_url;
    std::string new_key = kDriveMetadataKeyPrefix + origin.spec() +
        kMetadataKeySeparator + path.AsUTF8Unsafe();

    write_batch.Put(new_key, itr->value());
    write_batch.Delete(key);
  }

  return LevelDBStatusToSyncStatusCode(
      db->Write(leveldb::WriteOptions(), &write_batch));
}

SyncStatusCode MigrateDatabaseFromV1ToV2(leveldb::DB* db) {
  // Strips prefix of WAPI resource ID, and discards batch sync origins.
  // (i.e. "file:xxxx" => "xxxx", "folder:yyyy" => "yyyy")
  //
  // Version 2 database format (changed keys/fields are marked with '*'):
  //   key: "VERSION"
  // * value: 2
  //
  //   key: "CHANGE_STAMP"
  //   value: <Largest Changestamp>
  //
  //   key: "SYNC_ROOT_DIR"
  // * value: <Resource ID of the sync root directory> (striped)
  //
  //   key: "METADATA: " + <Origin and URL>
  // * value: <Serialized DriveMetadata> (stripped)
  //
  // * key: "BSYNC_ORIGIN: " + <URL string of a batch sync origin> (deleted)
  // * value: <Resource ID of the drive directory for the origin> (deleted)
  //
  //   key: "ISYNC_ORIGIN: " + <URL string of a incremental sync origin>
  // * value: <Resource ID of the drive directory for the origin> (stripped)
  //
  //   key: "DISABLED_ORIGIN: " + <URL string of a disabled origin>
  // * value: <Resource ID of the drive directory for the origin> (stripped)

  const char kDatabaseVersionKey[] = "VERSION";
  const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
  const char kDriveMetadataKeyPrefix[] = "METADATA: ";
  const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
  const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
  const char kDriveDisabledOriginKeyPrefix[] = "DISABLED_ORIGIN: ";

  leveldb::WriteBatch write_batch;
  write_batch.Put(kDatabaseVersionKey, "2");

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();

    // Strip resource id for the sync root directory.
    if (StartsWithASCII(key, kSyncRootDirectoryKey, true)) {
      write_batch.Put(key, RemoveWapiIdPrefix(itr->value().ToString()));
      continue;
    }

    // Strip resource ids in the drive metadata.
    if (StartsWithASCII(key, kDriveMetadataKeyPrefix, true)) {
      DriveMetadata metadata;
      bool success = metadata.ParseFromString(itr->value().ToString());
      DCHECK(success);

      metadata.set_resource_id(RemoveWapiIdPrefix(metadata.resource_id()));
      std::string metadata_string;
      metadata.SerializeToString(&metadata_string);

      write_batch.Put(key, metadata_string);
      continue;
    }

    // Deprecate legacy batch sync origin entries that are no longer needed.
    if (StartsWithASCII(key, kDriveBatchSyncOriginKeyPrefix, true)) {
      write_batch.Delete(key);
      continue;
    }

    // Strip resource ids of the incremental sync origins.
    if (StartsWithASCII(key, kDriveIncrementalSyncOriginKeyPrefix, true)) {
      write_batch.Put(key, RemoveWapiIdPrefix(itr->value().ToString()));
      continue;
    }

    // Strip resource ids of the disabled sync origins.
    if (StartsWithASCII(key, kDriveDisabledOriginKeyPrefix, true)) {
      write_batch.Put(key, RemoveWapiIdPrefix(itr->value().ToString()));
      continue;
    }
  }

  return LevelDBStatusToSyncStatusCode(
      db->Write(leveldb::WriteOptions(), &write_batch));
}

SyncStatusCode MigrateDatabaseFromV4ToV3(leveldb::DB* db) {
  // Rollback from version 4 to version 3.
  // Please see metadata_database_index.cc for version 3 format, and
  // metadata_database_index_on_disk.cc for version 4 format.

  const char kDatabaseVersionKey[] = "VERSION";
  const char kServiceMetadataKey[] = "SERVICE";
  const char kFileMetadataKeyPrefix[] = "FILE: ";
  const char kFileTrackerKeyPrefix[] = "TRACKER: ";

  // Key prefixes used in version 4.
  const char kAppRootIDByAppIDKeyPrefix[] = "APP_ROOT: ";
  const char kActiveTrackerIDByFileIDKeyPrefix[] = "ACTIVE_FILE: ";
  const char kTrackerIDByFileIDKeyPrefix[] = "TRACKER_FILE: ";
  const char kMultiTrackerByFileIDKeyPrefix[] = "MULTI_FILE: ";
  const char kActiveTrackerIDByParentAndTitleKeyPrefix[] = "ACTIVE_PATH: ";
  const char kTrackerIDByParentAndTitleKeyPrefix[] = "TRACKER_PATH: ";
  const char kMultiBackingParentAndTitleKeyPrefix[] = "MULTI_PATH: ";
  const char kDirtyIDKeyPrefix[] = "DIRTY: ";
  const char kDemotedDirtyIDKeyPrefix[] = "DEMOTED_DIRTY: ";

  leveldb::WriteBatch write_batch;
  write_batch.Put(kDatabaseVersionKey, "3");

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();

    // Do nothing for valid entries in both versions.
    if (StartsWithASCII(key, kServiceMetadataKey, true) ||
        StartsWithASCII(key, kFileMetadataKeyPrefix, true) ||
        StartsWithASCII(key, kFileTrackerKeyPrefix, true)) {
      continue;
    }

    // Drop entries used in version 4 only.
    if (StartsWithASCII(key, kAppRootIDByAppIDKeyPrefix, true) ||
        StartsWithASCII(key, kActiveTrackerIDByFileIDKeyPrefix, true) ||
        StartsWithASCII(key, kTrackerIDByFileIDKeyPrefix, true) ||
        StartsWithASCII(key, kMultiTrackerByFileIDKeyPrefix, true) ||
        StartsWithASCII(key, kActiveTrackerIDByParentAndTitleKeyPrefix, true) ||
        StartsWithASCII(key, kTrackerIDByParentAndTitleKeyPrefix, true) ||
        StartsWithASCII(key, kMultiBackingParentAndTitleKeyPrefix, true) ||
        StartsWithASCII(key, kDirtyIDKeyPrefix, true) ||
        StartsWithASCII(key, kDemotedDirtyIDKeyPrefix, true)) {
      write_batch.Delete(key);
      continue;
    }

    DVLOG(3) << "Unknown key: " << key << " was found.";
  }

  return LevelDBStatusToSyncStatusCode(
      db->Write(leveldb::WriteOptions(), &write_batch));
}

}  // namespace drive_backend
}  // namespace sync_file_system
