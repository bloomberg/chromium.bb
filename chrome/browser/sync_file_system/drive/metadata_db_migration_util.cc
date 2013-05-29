// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive/metadata_db_migration_util.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace sync_file_system {
namespace drive {

namespace {

const char kWapiFileIdPrefix[] = "file:";
const char kWapiFolderIdPrefix[] = "folder:";

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWithASCII(str, prefix, true))
    return std::string(str.begin() + prefix.size(), str.end());
  return str;
}

}  // namespace

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
  if (StartsWithASCII(resource_id, kWapiFileIdPrefix, true))
    return RemovePrefix(resource_id, kWapiFileIdPrefix);
  if (StartsWithASCII(resource_id, kWapiFolderIdPrefix, true))
    return RemovePrefix(resource_id, kWapiFolderIdPrefix);
  return resource_id;
}

SyncStatusCode MigrateDatabaseFromV1ToV2(leveldb::DB* db) {
  // Strips prefix of WAPI resource ID.
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
  // * value: <Serialized DriveMetadata> (striped)
  //
  //   key: "ISYNC_ORIGIN: " + <URL string of a incremental sync origin>
  // * value: <Resource ID of the drive directory for the origin> (striped)
  //
  //   key: "DISABLED_ORIGIN: " + <URL string of a disabled origin>
  // * value: <Resource ID of the drive directory for the origin> (striped)

  const char kDatabaseVersionKey[] = "VERSION";
  const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
  const char kDriveMetadataKeyPrefix[] = "METADATA: ";
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

}  // namespace drive
}  // namespace sync_file_system
