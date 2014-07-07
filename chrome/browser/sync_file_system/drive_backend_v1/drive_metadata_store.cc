// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

typedef DriveMetadataStore::MetadataMap MetadataMap;
typedef DriveMetadataStore::OriginByResourceId OriginByResourceId;
typedef DriveMetadataStore::PathToMetadata PathToMetadata;
typedef DriveMetadataStore::ResourceIdByOrigin ResourceIdByOrigin;

const base::FilePath::CharType DriveMetadataStore::kDatabaseName[] =
    FILE_PATH_LITERAL("DriveMetadata");

struct DBContents {
  SyncStatusCode status;
  scoped_ptr<leveldb::DB> db;
  bool created;

  int64 largest_changestamp;
  DriveMetadataStore::MetadataMap metadata_map;
  std::string sync_root_directory_resource_id;
  ResourceIdByOrigin incremental_sync_origins;
  ResourceIdByOrigin disabled_origins;

  DBContents()
      : status(SYNC_STATUS_UNKNOWN),
        created(false),
        largest_changestamp(0) {
  }
};

namespace {

const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 2;
const char kChangeStampKey[] = "CHANGE_STAMP";
const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
const char kDriveMetadataKeyPrefix[] = "METADATA: ";
const char kMetadataKeySeparator = ' ';
const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
const char kDriveDisabledOriginKeyPrefix[] = "DISABLED_ORIGIN: ";

enum OriginSyncType {
  INCREMENTAL_SYNC_ORIGIN,
  DISABLED_ORIGIN
};

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWithASCII(str, prefix, true))
    return str.substr(prefix.size());
  return str;
}

std::string OriginAndPathToMetadataKey(const GURL& origin,
                                       const base::FilePath& path) {
  return kDriveMetadataKeyPrefix + origin.spec() +
      kMetadataKeySeparator + path.AsUTF8Unsafe();
}

std::string FileSystemURLToMetadataKey(const FileSystemURL& url) {
  return OriginAndPathToMetadataKey(url.origin(), url.path());
}

void MetadataKeyToOriginAndPath(const std::string& metadata_key,
                                GURL* origin,
                                base::FilePath* path) {
  std::string key_body(RemovePrefix(metadata_key, kDriveMetadataKeyPrefix));
  size_t separator_position = key_body.find(kMetadataKeySeparator);
  *origin = GURL(key_body.substr(0, separator_position));
  *path = base::FilePath::FromUTF8Unsafe(
      key_body.substr(separator_position + 1));
}

bool UpdateResourceIdMap(ResourceIdByOrigin* map,
                         OriginByResourceId* reverse_map,
                         const GURL& origin,
                         const std::string& resource_id) {
  ResourceIdByOrigin::iterator found = map->find(origin);
  if (found == map->end())
    return false;
  reverse_map->erase(found->second);
  reverse_map->insert(std::make_pair(resource_id, origin));

  found->second = resource_id;
  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool IsDBEmpty(leveldb::DB* db) {
  DCHECK(db);
  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

scoped_ptr<leveldb::DB> OpenDatabase(const base::FilePath& path,
                                     SyncStatusCode* status,
                                     bool* created) {
  DCHECK(status);
  DCHECK(created);

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status db_status = leveldb::DB::Open(
      options, path.AsUTF8Unsafe(), &db);
  if (db_status.ok()) {
    *created = IsDBEmpty(db);
  } else {
    delete db;
    db = NULL;
  }
  *status = LevelDBStatusToSyncStatusCode(db_status);

  return make_scoped_ptr(db);
}

SyncStatusCode WriteInitialData(leveldb::DB* db) {
  DCHECK(db);
  return LevelDBStatusToSyncStatusCode(db->Put(
      leveldb::WriteOptions(),
      kDatabaseVersionKey,
      base::Int64ToString(kCurrentDatabaseVersion)));
}

SyncStatusCode MigrateDatabaseIfNeeded(leveldb::DB* db) {
  DCHECK(db);
  std::string value;
  leveldb::Status status = db->Get(leveldb::ReadOptions(),
                                   kDatabaseVersionKey, &value);
  int64 version = 0;
  if (status.ok()) {
    if (!base::StringToInt64(value, &version))
      return SYNC_DATABASE_ERROR_FAILED;
  } else {
    if (!status.IsNotFound())
      return SYNC_DATABASE_ERROR_FAILED;
  }

  switch (version) {
    case 0:
      drive_backend::MigrateDatabaseFromV0ToV1(db);
      // fall-through
    case 1:
      drive_backend::MigrateDatabaseFromV1ToV2(db);
      // fall-through
    case 2:
      DCHECK_EQ(2, kCurrentDatabaseVersion);
      return SYNC_STATUS_OK;
    default:
      return SYNC_DATABASE_ERROR_FAILED;
  }
}

SyncStatusCode ReadContents(DBContents* contents) {
  DCHECK(contents);
  DCHECK(contents->db);

  contents->largest_changestamp = 0;
  contents->metadata_map.clear();
  contents->incremental_sync_origins.clear();

  scoped_ptr<leveldb::Iterator> itr(
      contents->db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (key == kChangeStampKey) {
      bool success = base::StringToInt64(itr->value().ToString(),
                                         &contents->largest_changestamp);
      DCHECK(success);
      continue;
    }

    if (key == kSyncRootDirectoryKey) {
      std::string resource_id = itr->value().ToString();
      if (IsDriveAPIDisabled())
        resource_id = drive_backend::AddWapiFolderPrefix(resource_id);
      contents->sync_root_directory_resource_id = resource_id;
      continue;
    }

    if (StartsWithASCII(key, kDriveMetadataKeyPrefix, true)) {
      GURL origin;
      base::FilePath path;
      MetadataKeyToOriginAndPath(key, &origin, &path);

      DriveMetadata metadata;
      bool success = metadata.ParseFromString(itr->value().ToString());
      DCHECK(success);

      if (IsDriveAPIDisabled()) {
        metadata.set_resource_id(drive_backend::AddWapiIdPrefix(
            metadata.resource_id(), metadata.type()));
      }

      success = contents->metadata_map[origin].insert(
          std::make_pair(path, metadata)).second;
      DCHECK(success);
      continue;
    }

    if (StartsWithASCII(key, kDriveIncrementalSyncOriginKeyPrefix, true)) {
      GURL origin(RemovePrefix(key, kDriveIncrementalSyncOriginKeyPrefix));
      DCHECK(origin.is_valid());

      std::string origin_resource_id = IsDriveAPIDisabled()
          ? drive_backend::AddWapiFolderPrefix(itr->value().ToString())
          : itr->value().ToString();

      DCHECK(!ContainsKey(contents->incremental_sync_origins, origin));
      contents->incremental_sync_origins[origin] = origin_resource_id;
      continue;
    }

    if (StartsWithASCII(key, kDriveDisabledOriginKeyPrefix, true)) {
      GURL origin(RemovePrefix(key, kDriveDisabledOriginKeyPrefix));
      DCHECK(origin.is_valid());

      std::string origin_resource_id = IsDriveAPIDisabled()
          ? drive_backend::AddWapiFolderPrefix(itr->value().ToString())
          : itr->value().ToString();

      DCHECK(!ContainsKey(contents->disabled_origins, origin));
      contents->disabled_origins[origin] = origin_resource_id;
      continue;
    }
  }

  return SYNC_STATUS_OK;
}

scoped_ptr<DBContents> LoadDBContents(const base::FilePath& db_path) {
  scoped_ptr<DBContents> contents(new DBContents);
  contents->db = OpenDatabase(db_path,
                              &contents->status,
                              &contents->created);
  if (contents->status != SYNC_STATUS_OK)
    return contents.Pass();

  if (contents->created) {
    contents->status = WriteInitialData(contents->db.get());
    if (contents->status != SYNC_STATUS_OK)
      return contents.Pass();
  } else {
    contents->status = MigrateDatabaseIfNeeded(contents->db.get());
    if (contents->status != SYNC_STATUS_OK)
      return contents.Pass();
  }

  contents->status = ReadContents(contents.get());
  return contents.Pass();
}

////////////////////////////////////////////////////////////////////////////////

// Returns a key string for the given origin.
// For example, when |origin| is "http://www.example.com" and |sync_type| is
// BATCH_SYNC_ORIGIN, returns "BSYNC_ORIGIN: http://www.example.com".
std::string CreateKeyForOriginRoot(const GURL& origin,
                                   OriginSyncType sync_type) {
  DCHECK(origin.is_valid());
  switch (sync_type) {
    case INCREMENTAL_SYNC_ORIGIN:
      return kDriveIncrementalSyncOriginKeyPrefix + origin.spec();
    case DISABLED_ORIGIN:
      return kDriveDisabledOriginKeyPrefix + origin.spec();
  }
  NOTREACHED();
  return std::string();
}

void AddOriginsToVector(std::vector<GURL>* all_origins,
                        const ResourceIdByOrigin& resource_map) {
  for (ResourceIdByOrigin::const_iterator itr = resource_map.begin();
       itr != resource_map.end();
       ++itr) {
    all_origins->push_back(itr->first);
  }
}

void InsertReverseMap(const ResourceIdByOrigin& forward_map,
                      OriginByResourceId* backward_map) {
  for (ResourceIdByOrigin::const_iterator itr = forward_map.begin();
       itr != forward_map.end(); ++itr)
    backward_map->insert(std::make_pair(itr->second, itr->first));
}

bool EraseIfExists(ResourceIdByOrigin* map,
                   const GURL& origin,
                   std::string* resource_id) {
  ResourceIdByOrigin::iterator found = map->find(origin);
  if (found == map->end())
    return false;
  *resource_id = found->second;
  map->erase(found);
  return true;
}

void AppendMetadataDeletionToBatch(const MetadataMap& metadata_map,
                                   const GURL& origin,
                                   leveldb::WriteBatch* batch) {
  MetadataMap::const_iterator found = metadata_map.find(origin);
  if (found == metadata_map.end())
    return;

  for (PathToMetadata::const_iterator itr = found->second.begin();
       itr != found->second.end(); ++itr)
    batch->Delete(OriginAndPathToMetadataKey(origin, itr->first));
}

std::string DriveTypeToString(DriveMetadata_ResourceType drive_type) {
  switch (drive_type) {
    case DriveMetadata_ResourceType_RESOURCE_TYPE_FILE:
      return "file";
    case DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER:
      return "folder";
  }

  NOTREACHED();
  return "unknown";
}

}  // namespace

DriveMetadataStore::DriveMetadataStore(
    const base::FilePath& base_dir,
    base::SequencedTaskRunner* file_task_runner)
    : file_task_runner_(file_task_runner),
      base_dir_(base_dir),
      db_status_(SYNC_STATUS_UNKNOWN),
      largest_changestamp_(0) {
  DCHECK(file_task_runner);
}

DriveMetadataStore::~DriveMetadataStore() {
  DCHECK(CalledOnValidThread());
  file_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

void DriveMetadataStore::Initialize(const InitializationCallback& callback) {
  DCHECK(CalledOnValidThread());
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&LoadDBContents, base_dir_.Append(kDatabaseName)),
      base::Bind(&DriveMetadataStore::DidInitialize, AsWeakPtr(), callback));
}

void DriveMetadataStore::DidInitialize(const InitializationCallback& callback,
                                       scoped_ptr<DBContents> contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(contents);

  db_status_ = contents->status;
  if (db_status_ != SYNC_STATUS_OK) {
    callback.Run(db_status_, false);
    file_task_runner_->DeleteSoon(FROM_HERE, contents.release());
    return;
  }

  db_ = contents->db.Pass();
  largest_changestamp_ = contents->largest_changestamp;
  metadata_map_.swap(contents->metadata_map);
  sync_root_directory_resource_id_ = contents->sync_root_directory_resource_id;
  incremental_sync_origins_.swap(contents->incremental_sync_origins);
  disabled_origins_.swap(contents->disabled_origins);

  origin_by_resource_id_.clear();
  InsertReverseMap(incremental_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(disabled_origins_, &origin_by_resource_id_);

  callback.Run(db_status_, contents->created);
}

void DriveMetadataStore::SetLargestChangeStamp(
    int64 largest_changestamp,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);
  largest_changestamp_ = largest_changestamp;

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Put(kChangeStampKey, base::Int64ToString(largest_changestamp));
  return WriteToDB(batch.Pass(), callback);
}

int64 DriveMetadataStore::GetLargestChangeStamp() const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);
  return largest_changestamp_;
}

void DriveMetadataStore::UpdateEntry(
    const FileSystemURL& url,
    const DriveMetadata& metadata,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);
  DCHECK(!metadata.conflicted() || !metadata.to_be_fetched());

  std::pair<PathToMetadata::iterator, bool> result =
      metadata_map_[url.origin()].insert(std::make_pair(url.path(), metadata));
  if (!result.second)
    result.first->second = metadata;

  std::string value;
  if (IsDriveAPIDisabled()) {
    DriveMetadata metadata_in_db(metadata);
    metadata_in_db.set_resource_id(
        drive_backend::RemoveWapiIdPrefix(metadata.resource_id()));
    bool success = metadata_in_db.SerializeToString(&value);
    DCHECK(success);
  } else {
    bool success = metadata.SerializeToString(&value);
    DCHECK(success);
  }

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Put(FileSystemURLToMetadataKey(url), value);
  WriteToDB(batch.Pass(), callback);
}

void DriveMetadataStore::DeleteEntry(
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  MetadataMap::iterator found = metadata_map_.find(url.origin());
  if (found == metadata_map_.end()) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (found->second.erase(url.path()) == 1) {
    scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
    batch->Delete(FileSystemURLToMetadataKey(url));
    WriteToDB(batch.Pass(), callback);
    return;
  }

  RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
}

SyncStatusCode DriveMetadataStore::ReadEntry(const FileSystemURL& url,
                                             DriveMetadata* metadata) const {
  DCHECK(CalledOnValidThread());
  DCHECK(metadata);

  MetadataMap::const_iterator found_origin = metadata_map_.find(url.origin());
  if (found_origin == metadata_map_.end())
    return SYNC_DATABASE_ERROR_NOT_FOUND;

  PathToMetadata::const_iterator found = found_origin->second.find(url.path());
  if (found == found_origin->second.end())
    return SYNC_DATABASE_ERROR_NOT_FOUND;

  *metadata = found->second;
  return SYNC_STATUS_OK;
}

void DriveMetadataStore::AddIncrementalSyncOrigin(
    const GURL& origin,
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsIncrementalSyncOrigin(origin));
  DCHECK(!IsOriginDisabled(origin));
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  incremental_sync_origins_.insert(std::make_pair(origin, resource_id));
  origin_by_resource_id_.insert(std::make_pair(resource_id, origin));

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Delete(CreateKeyForOriginRoot(origin, DISABLED_ORIGIN));
  batch->Put(CreateKeyForOriginRoot(origin, INCREMENTAL_SYNC_ORIGIN),
             drive_backend::RemoveWapiIdPrefix(resource_id));
  WriteToDB(batch.Pass(),
            base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

void DriveMetadataStore::SetSyncRootDirectory(const std::string& resource_id) {
  DCHECK(CalledOnValidThread());

  sync_root_directory_resource_id_ = resource_id;

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Put(kSyncRootDirectoryKey,
             drive_backend::RemoveWapiIdPrefix(resource_id));
  return WriteToDB(batch.Pass(),
                   base::Bind(&DriveMetadataStore::UpdateDBStatus,
                              AsWeakPtr()));
}

void DriveMetadataStore::SetOriginRootDirectory(
    const GURL& origin,
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsKnownOrigin(origin));

  OriginSyncType sync_type;
  if (UpdateResourceIdMap(
      &incremental_sync_origins_, &origin_by_resource_id_,
      origin, resource_id)) {
    sync_type = INCREMENTAL_SYNC_ORIGIN;
  } else if (UpdateResourceIdMap(&disabled_origins_, &origin_by_resource_id_,
                                 origin, resource_id)) {
    sync_type = DISABLED_ORIGIN;
  } else {
    return;
  }

  std::string key = CreateKeyForOriginRoot(origin, sync_type);
  DCHECK(!key.empty());

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Put(key, drive_backend::RemoveWapiIdPrefix(resource_id));
  WriteToDB(batch.Pass(),
            base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

bool DriveMetadataStore::IsKnownOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return IsIncrementalSyncOrigin(origin) || IsOriginDisabled(origin);
}

bool DriveMetadataStore::IsIncrementalSyncOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(incremental_sync_origins_, origin);
}

bool DriveMetadataStore::IsOriginDisabled(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(disabled_origins_, origin);
}

void DriveMetadataStore::EnableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::map<GURL, std::string>::iterator found = disabled_origins_.find(origin);
  if (found == disabled_origins_.end()) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    // |origin| has not been registered yet.
    return;
  }
  disabled_origins_.erase(found);

  // |origin| goes back to DriveFileSyncService::pending_batch_sync_origins_
  // only and is not stored in drive_metadata_store.
  found = incremental_sync_origins_.find(origin);
  if (found != incremental_sync_origins_.end())
    incremental_sync_origins_.erase(found);

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Delete(CreateKeyForOriginRoot(origin, INCREMENTAL_SYNC_ORIGIN));
  batch->Delete(CreateKeyForOriginRoot(origin, DISABLED_ORIGIN));
  WriteToDB(batch.Pass(), callback);
}

void DriveMetadataStore::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string resource_id;
  if (!EraseIfExists(&incremental_sync_origins_, origin, &resource_id)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }
  disabled_origins_[origin] = resource_id;

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Delete(CreateKeyForOriginRoot(origin, INCREMENTAL_SYNC_ORIGIN));
  batch->Put(CreateKeyForOriginRoot(origin, DISABLED_ORIGIN),
             drive_backend::RemoveWapiIdPrefix(resource_id));
  AppendMetadataDeletionToBatch(metadata_map_, origin, batch.get());
  metadata_map_.erase(origin);

  WriteToDB(batch.Pass(), callback);
}

void DriveMetadataStore::RemoveOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string resource_id;
  if (!EraseIfExists(&incremental_sync_origins_, origin, &resource_id) &&
      !EraseIfExists(&disabled_origins_, origin, &resource_id)) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }
  origin_by_resource_id_.erase(resource_id);

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Delete(CreateKeyForOriginRoot(origin, INCREMENTAL_SYNC_ORIGIN));
  batch->Delete(CreateKeyForOriginRoot(origin, DISABLED_ORIGIN));
  AppendMetadataDeletionToBatch(metadata_map_, origin, batch.get());
  metadata_map_.erase(origin);

  WriteToDB(batch.Pass(), callback);
}

void DriveMetadataStore::WriteToDB(scoped_ptr<leveldb::WriteBatch> batch,
                                   const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (db_status_ != SYNC_STATUS_OK &&
      db_status_ != SYNC_DATABASE_ERROR_NOT_FOUND) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_DATABASE_ERROR_FAILED));
    return;
  }

  DCHECK(db_);
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&leveldb::DB::Write,
                 base::Unretained(db_.get()),
                 leveldb::WriteOptions(),
                 base::Owned(batch.release())),
      base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                 AsWeakPtr(),
                 callback));
}

void DriveMetadataStore::UpdateDBStatus(SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  if (db_status_ != SYNC_STATUS_OK &&
      db_status_ != SYNC_DATABASE_ERROR_NOT_FOUND) {
    // TODO(tzik): Handle database corruption. http://crbug.com/153709
    db_status_ = status;
    util::Log(logging::LOG_WARNING,
              FROM_HERE,
              "DriveMetadataStore turned to wrong state: %s",
              SyncStatusCodeToString(status));
    return;
  }
  db_status_ = SYNC_STATUS_OK;
}

void DriveMetadataStore::UpdateDBStatusAndInvokeCallback(
    const SyncStatusCallback& callback,
    const leveldb::Status& leveldb_status) {
  SyncStatusCode status = LevelDBStatusToSyncStatusCode(leveldb_status);
  UpdateDBStatus(status);
  callback.Run(status);
}

SyncStatusCode DriveMetadataStore::GetConflictURLs(
    fileapi::FileSystemURLSet* urls) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  urls->clear();
  for (MetadataMap::const_iterator origin_itr = metadata_map_.begin();
       origin_itr != metadata_map_.end();
       ++origin_itr) {
    for (PathToMetadata::const_iterator itr = origin_itr->second.begin();
         itr != origin_itr->second.end();
         ++itr) {
      if (itr->second.conflicted()) {
        urls->insert(CreateSyncableFileSystemURL(
            origin_itr->first, itr->first));
      }
    }
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode DriveMetadataStore::GetToBeFetchedFiles(
    URLAndDriveMetadataList* list) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  list->clear();
  for (MetadataMap::const_iterator origin_itr = metadata_map_.begin();
       origin_itr != metadata_map_.end();
       ++origin_itr) {
    for (PathToMetadata::const_iterator itr = origin_itr->second.begin();
         itr != origin_itr->second.end();
         ++itr) {
      if (itr->second.to_be_fetched()) {
        FileSystemURL url = CreateSyncableFileSystemURL(
            origin_itr->first, itr->first);
        list->push_back(std::make_pair(url, itr->second));
      }
    }
  }
  return SYNC_STATUS_OK;
}

std::string DriveMetadataStore::GetResourceIdForOrigin(
    const GURL& origin) const {
  DCHECK(CalledOnValidThread());

  // If we don't have valid root directory (this could be reset even after
  // initialized) just return empty string, as the origin directories
  // in the root directory must have become invalid now too.
  if (sync_root_directory().empty())
    return std::string();

  ResourceIdByOrigin::const_iterator found =
      incremental_sync_origins_.find(origin);
  if (found != incremental_sync_origins_.end())
    return found->second;

  found = disabled_origins_.find(origin);
  if (found != disabled_origins_.end())
    return found->second;

  return std::string();
}

void DriveMetadataStore::GetAllOrigins(std::vector<GURL>* origins) {
  DCHECK(CalledOnValidThread());
  DCHECK(origins);
  origins->clear();
  origins->reserve(incremental_sync_origins_.size() +
                   disabled_origins_.size());
  AddOriginsToVector(origins, incremental_sync_origins_);
  AddOriginsToVector(origins, disabled_origins_);
}

bool DriveMetadataStore::GetOriginByOriginRootDirectoryId(
    const std::string& resource_id,
    GURL* origin) {
  DCHECK(CalledOnValidThread());
  DCHECK(origin);

  OriginByResourceId::iterator found = origin_by_resource_id_.find(resource_id);
  if (found == origin_by_resource_id_.end())
    return false;
  *origin = found->second;
  return true;
}

scoped_ptr<base::ListValue> DriveMetadataStore::DumpFiles(const GURL& origin) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<base::ListValue> files(new base::ListValue);

  MetadataMap::const_iterator found = metadata_map_.find(origin);
  if (found == metadata_map_.end())
    return make_scoped_ptr(new base::ListValue);

  for (PathToMetadata::const_iterator itr = found->second.begin();
       itr != found->second.end();
       ++itr) {
    // Convert Drive specific metadata to Common File metadata object.
    const DriveMetadata& metadata = itr->second;

    base::DictionaryValue* file = new base::DictionaryValue;
    file->SetString("path", itr->first.AsUTF8Unsafe());
    file->SetString("title", itr->first.BaseName().AsUTF8Unsafe());
    file->SetString("type", DriveTypeToString(metadata.type()));

    base::DictionaryValue* details = new base::DictionaryValue;
    details->SetString("resource_id", metadata.resource_id());
    details->SetString("md5", metadata.md5_checksum());
    details->SetString("dirty", metadata.to_be_fetched() ? "true" : "false");

    file->Set("details", details);
    files->Append(file);
  }

  return files.Pass();
}

}  // namespace sync_file_system
