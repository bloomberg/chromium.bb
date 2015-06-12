// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

namespace sync_file_system {
namespace drive_backend {

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
    if (base::StartsWithASCII(key, kServiceMetadataKey, true) ||
        base::StartsWithASCII(key, kFileMetadataKeyPrefix, true) ||
        base::StartsWithASCII(key, kFileTrackerKeyPrefix, true)) {
      continue;
    }

    // Drop entries used in version 4 only.
    if (base::StartsWithASCII(key, kAppRootIDByAppIDKeyPrefix, true) ||
        base::StartsWithASCII(key, kActiveTrackerIDByFileIDKeyPrefix, true) ||
        base::StartsWithASCII(key, kTrackerIDByFileIDKeyPrefix, true) ||
        base::StartsWithASCII(key, kMultiTrackerByFileIDKeyPrefix, true) ||
        base::StartsWithASCII(key, kActiveTrackerIDByParentAndTitleKeyPrefix,
                              true) ||
        base::StartsWithASCII(key, kTrackerIDByParentAndTitleKeyPrefix, true) ||
        base::StartsWithASCII(key, kMultiBackingParentAndTitleKeyPrefix,
                              true) ||
        base::StartsWithASCII(key, kDirtyIDKeyPrefix, true) ||
        base::StartsWithASCII(key, kDemotedDirtyIDKeyPrefix, true)) {
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
