// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/sync_file_system/drive/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "googleurl/src/gurl.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

typedef DriveMetadataStore::ResourceIdByOrigin ResourceIdByOrigin;
typedef DriveMetadataStore::OriginByResourceId OriginByResourceId;

const base::FilePath::CharType DriveMetadataStore::kDatabaseName[] =
    FILE_PATH_LITERAL("DriveMetadata");

namespace {

const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 2;
const char kChangeStampKey[] = "CHANGE_STAMP";
const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
const char kDriveMetadataKeyPrefix[] = "METADATA: ";
const char kMetadataKeySeparator = ' ';
const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
const char kDriveDisabledOriginKeyPrefix[] = "DISABLED_ORIGIN: ";
const size_t kDriveMetadataKeyPrefixLength = arraysize(kDriveMetadataKeyPrefix);

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWithASCII(str, prefix, true))
    return str.substr(prefix.size());
  return str;
}

std::string FileSystemURLToMetadataKey(const FileSystemURL& url) {
  return kDriveMetadataKeyPrefix + url.origin().spec() +
      kMetadataKeySeparator + url.path().AsUTF8Unsafe();
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

}  // namespace

class DriveMetadataDB {
 public:
  enum OriginSyncType {
    INCREMENTAL_SYNC_ORIGIN,
    DISABLED_ORIGIN
  };

  typedef DriveMetadataStore::MetadataMap MetadataMap;

  DriveMetadataDB(const base::FilePath& base_dir,
                  base::SequencedTaskRunner* task_runner);
  ~DriveMetadataDB();

  SyncStatusCode Initialize(bool* created);
  SyncStatusCode ReadContents(DriveMetadataDBContents* contents);

  SyncStatusCode MigrateDatabaseIfNeeded();

  SyncStatusCode SetLargestChangestamp(int64 largest_changestamp);
  SyncStatusCode SetSyncRootDirectory(const std::string& resource_id);
  SyncStatusCode GetSyncRootDirectory(std::string* resource_id);
  SyncStatusCode SetOriginRootDirectory(const GURL& origin,
                                        OriginSyncType sync_type,
                                        const std::string& resource_id);
  SyncStatusCode UpdateEntry(const FileSystemURL& url,
                             DriveMetadata metadata);

  // TODO(calvinlo): consolidate these state transition functions for sync
  // origins like "UpdateOrigin(GURL, SyncStatusEnum)". And manage origins in
  // just one map like "Map<SyncStatusEnum, ResourceIDMap>".
  // http://crbug.com/211600
  SyncStatusCode UpdateOriginAsIncrementalSync(const GURL& origin,
                                               const std::string& resource_id);
  SyncStatusCode EnableOrigin(const GURL& origin,
                              const std::string& resource_id);
  SyncStatusCode DisableOrigin(const GURL& origin,
                               const std::string& resource_id);
  SyncStatusCode RemoveOrigin(const GURL& origin);

  SyncStatusCode GetOrigins(ResourceIdByOrigin* incremental_sync_origins,
                            ResourceIdByOrigin* disabled_origins);

  SyncStatusCode WriteToDB(leveldb::WriteBatch* batch) {
    return LevelDBStatusToSyncStatusCode(db_->Write(
        leveldb::WriteOptions(), batch));
  }

 private:
  friend class DriveMetadataStore;

  bool CalledOnValidThread() const {
    return task_runner_->RunsTasksOnCurrentThread();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::string db_path_;
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataDB);
};

struct DriveMetadataDBContents {
  int64 largest_changestamp;
  DriveMetadataStore::MetadataMap metadata_map;
  std::string sync_root_directory_resource_id;
  ResourceIdByOrigin incremental_sync_origins;
  ResourceIdByOrigin disabled_origins;
};

namespace {

SyncStatusCode InitializeDBOnFileThread(DriveMetadataDB* db,
                                        DriveMetadataDBContents* contents) {
  DCHECK(db);
  DCHECK(contents);

  contents->largest_changestamp = 0;
  contents->metadata_map.clear();
  contents->incremental_sync_origins.clear();
  contents->disabled_origins.clear();

  bool created = false;
  SyncStatusCode status = db->Initialize(&created);
  if (status != SYNC_STATUS_OK)
    return status;

  if (!created) {
    status = db->MigrateDatabaseIfNeeded();
    if (status != SYNC_STATUS_OK) {
      util::Log(logging::LOG_WARNING,
                FROM_HERE,
                "Failed to migrate DriveMetadataStore to latest version.");
      return status;
    }
  }

  return db->ReadContents(contents);
}

// Returns a key string for the given origin.
// For example, when |origin| is "http://www.example.com" and |sync_type| is
// BATCH_SYNC_ORIGIN, returns "BSYNC_ORIGIN: http://www.example.com".
std::string CreateKeyForOriginRoot(const GURL& origin,
                                   DriveMetadataDB::OriginSyncType sync_type) {
  DCHECK(origin.is_valid());
  switch (sync_type) {
    case DriveMetadataDB::INCREMENTAL_SYNC_ORIGIN:
      return kDriveIncrementalSyncOriginKeyPrefix + origin.spec();
    case DriveMetadataDB::DISABLED_ORIGIN:
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

}  // namespace

DriveMetadataStore::DriveMetadataStore(
    const base::FilePath& base_dir,
    base::SequencedTaskRunner* file_task_runner)
    : file_task_runner_(file_task_runner),
      db_(new DriveMetadataDB(base_dir, file_task_runner)),
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
  DriveMetadataDBContents* contents = new DriveMetadataDBContents;

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(InitializeDBOnFileThread, db_.get(), contents),
      base::Bind(&DriveMetadataStore::DidInitialize,
                 AsWeakPtr(),
                 callback,
                 base::Owned(contents)));
}

void DriveMetadataStore::DidInitialize(const InitializationCallback& callback,
                                       DriveMetadataDBContents* contents,
                                       SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(contents);

  db_status_ = status;
  if (status != SYNC_STATUS_OK) {
    callback.Run(status, false);
    return;
  }

  largest_changestamp_ = contents->largest_changestamp;
  metadata_map_.swap(contents->metadata_map);
  sync_root_directory_resource_id_ = contents->sync_root_directory_resource_id;
  incremental_sync_origins_.swap(contents->incremental_sync_origins);
  disabled_origins_.swap(contents->disabled_origins);
  // |largest_changestamp_| is set to 0 for a fresh empty database.

  origin_by_resource_id_.clear();
  InsertReverseMap(incremental_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(disabled_origins_, &origin_by_resource_id_);

  callback.Run(status, largest_changestamp_ <= 0);
}

void DriveMetadataStore::RestoreSyncRootDirectory(
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string* sync_root_directory_resource_id = new std::string;
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::GetSyncRootDirectory,
                 base::Unretained(db_.get()),
                 sync_root_directory_resource_id),
      base::Bind(&DriveMetadataStore::DidRestoreSyncRootDirectory,
                 AsWeakPtr(),
                 callback,
                 base::Owned(sync_root_directory_resource_id)));
}

void DriveMetadataStore::DidRestoreSyncRootDirectory(
    const SyncStatusCallback& callback,
    std::string* sync_root_directory_resource_id,
    SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_root_directory_resource_id);

  db_status_ = status;
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  sync_root_directory_resource_id_.swap(*sync_root_directory_resource_id);
  callback.Run(status);
}

void DriveMetadataStore::RestoreOrigins(
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  ResourceIdByOrigin* incremental_sync_origins = new ResourceIdByOrigin;
  ResourceIdByOrigin* disabled_origins = new ResourceIdByOrigin;
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::GetOrigins,
                 base::Unretained(db_.get()),
                 incremental_sync_origins,
                 disabled_origins),
      base::Bind(&DriveMetadataStore::DidRestoreOrigins,
                 AsWeakPtr(),
                 callback,
                 base::Owned(incremental_sync_origins),
                 base::Owned(disabled_origins)));
}

void DriveMetadataStore::DidRestoreOrigins(
    const SyncStatusCallback& callback,
    ResourceIdByOrigin* incremental_sync_origins,
    ResourceIdByOrigin* disabled_origins,
    SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(incremental_sync_origins);
  DCHECK(disabled_origins);

  db_status_ = status;
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  incremental_sync_origins_.swap(*incremental_sync_origins);
  disabled_origins_.swap(*disabled_origins);

  origin_by_resource_id_.clear();
  InsertReverseMap(incremental_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(disabled_origins_, &origin_by_resource_id_);

  callback.Run(status);
}

leveldb::DB* DriveMetadataStore::GetDBInstanceForTesting() {
  return db_->db_.get();
}

void DriveMetadataStore::SetLargestChangeStamp(
    int64 largest_changestamp,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);
  largest_changestamp_ = largest_changestamp;
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::SetLargestChangestamp,
                 base::Unretained(db_.get()),
                 largest_changestamp),
      base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                 AsWeakPtr(),
                 callback));
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

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateEntry,
                 base::Unretained(db_.get()),
                 url,
                 metadata),
      base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                 AsWeakPtr(),
                 callback));
}

void DriveMetadataStore::DeleteEntry(
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  MetadataMap::iterator found = metadata_map_.find(url.origin());
  if (found == metadata_map_.end()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
    return;
  }

  if (found->second.erase(url.path()) == 1) {
    scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
    batch->Delete(FileSystemURLToMetadataKey(url));
    WriteToDB(batch.Pass(), callback);
    return;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_DATABASE_ERROR_NOT_FOUND));
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

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateOriginAsIncrementalSync,
                 base::Unretained(db_.get()),
                 origin,
                 resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

void DriveMetadataStore::SetSyncRootDirectory(const std::string& resource_id) {
  DCHECK(CalledOnValidThread());

  sync_root_directory_resource_id_ = resource_id;

  // Set the resource ID for the sync root directory in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::SetSyncRootDirectory,
                 base::Unretained(db_.get()),
                 resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

void DriveMetadataStore::SetOriginRootDirectory(
    const GURL& origin,
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsKnownOrigin(origin));

  DriveMetadataDB::OriginSyncType sync_type;
  if (UpdateResourceIdMap(
      &incremental_sync_origins_, &origin_by_resource_id_,
      origin, resource_id)) {
    sync_type = DriveMetadataDB::INCREMENTAL_SYNC_ORIGIN;
  } else if (UpdateResourceIdMap(&disabled_origins_, &origin_by_resource_id_,
                                 origin, resource_id)) {
    sync_type = DriveMetadataDB::DISABLED_ORIGIN;
  } else {
    return;
  }
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::SetOriginRootDirectory,
                 base::Unretained(db_.get()),
                 origin,
                 sync_type,
                 resource_id),
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
    // |origin| has not been registered yet.
    return;
  }
  std::string resource_id = found->second;
  disabled_origins_.erase(found);

  // |origin| goes back to DriveFileSyncService::pending_batch_sync_origins_
  // only and is not stored in drive_metadata_store.
  found = incremental_sync_origins_.find(origin);
  if (found != incremental_sync_origins_.end())
    incremental_sync_origins_.erase(found);

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::EnableOrigin,
                 base::Unretained(db_.get()),
                 origin,
                 resource_id),
      base::Bind(&DriveMetadataStore::DidUpdateOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string resource_id;
  std::map<GURL, std::string>::iterator found =
      incremental_sync_origins_.find(origin);
  if (found != incremental_sync_origins_.end()) {
    resource_id = found->second;
    incremental_sync_origins_.erase(found);
  }

  if (resource_id.empty()) {
    // |origin| has not been registered yet.
    return;
  }

  disabled_origins_.insert(std::make_pair(origin, resource_id));

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DriveMetadataDB::DisableOrigin,
                 base::Unretained(db_.get()),
                 origin,
                 resource_id),
      base::Bind(&DriveMetadataStore::DidUpdateOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::RemoveOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  metadata_map_.erase(origin);

  std::string resource_id;
  if (EraseIfExists(&incremental_sync_origins_, origin, &resource_id) ||
      EraseIfExists(&disabled_origins_, origin, &resource_id)) {
    origin_by_resource_id_.erase(resource_id);
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &DriveMetadataDB::RemoveOrigin, base::Unretained(db_.get()), origin),
      base::Bind(&DriveMetadataStore::DidUpdateOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::DidUpdateOrigin(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  UpdateDBStatus(status);
  callback.Run(status);
}

void DriveMetadataStore::WriteToDB(scoped_ptr<leveldb::WriteBatch> batch,
                                   const SyncStatusCallback& callback) {
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::WriteToDB,
                 base::Unretained(db_.get()), base::Owned(batch.release())),
      base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                 AsWeakPtr(), callback));
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
              SyncStatusCodeToString(status).c_str());
    return;
  }
  db_status_ = SYNC_STATUS_OK;
}

void DriveMetadataStore::UpdateDBStatusAndInvokeCallback(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
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

////////////////////////////////////////////////////////////////////////////////

DriveMetadataDB::DriveMetadataDB(const base::FilePath& base_dir,
                                 base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner),
      db_path_(fileapi::FilePathToString(
          base_dir.Append(DriveMetadataStore::kDatabaseName))) {
}

DriveMetadataDB::~DriveMetadataDB() {
  DCHECK(CalledOnValidThread());
}

SyncStatusCode DriveMetadataDB::Initialize(bool* created) {
  DCHECK(CalledOnValidThread());
  DCHECK(!db_.get());
  DCHECK(created);

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, db_path_, &db);
  // TODO(tzik): Handle database corruption. http://crbug.com/153709
  if (!status.ok()) {
    delete db;
    return LevelDBStatusToSyncStatusCode(status);
  }

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  *created = !itr->Valid();

  if (*created) {
    status = db->Put(leveldb::WriteOptions(),
                     kDatabaseVersionKey,
                     base::Int64ToString(kCurrentDatabaseVersion));
    if (!status.ok()) {
      delete db;
      return LevelDBStatusToSyncStatusCode(status);
    }
  }

  db_.reset(db);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::ReadContents(
    DriveMetadataDBContents* contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());
  DCHECK(contents);

  contents->largest_changestamp = 0;
  contents->metadata_map.clear();
  contents->incremental_sync_origins.clear();

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (key == kChangeStampKey) {
      bool success = base::StringToInt64(itr->value().ToString(),
                                         &contents->largest_changestamp);
      DCHECK(success);
    } else if (StartsWithASCII(key, kDriveMetadataKeyPrefix, true)) {
      GURL origin;
      base::FilePath path;
      MetadataKeyToOriginAndPath(key, &origin, &path);

      DriveMetadata metadata;
      bool success = metadata.ParseFromString(itr->value().ToString());
      DCHECK(success);

      if (!IsDriveAPIEnabled()) {
        metadata.set_resource_id(
            drive::AddWapiIdPrefix(metadata.resource_id(), metadata.type()));
      }

      success = contents->metadata_map[origin].insert(
          std::make_pair(path, metadata)).second;
      DCHECK(success);
    }
  }

  SyncStatusCode status = GetOrigins(&contents->incremental_sync_origins,
                                     &contents->disabled_origins);
  if (status != SYNC_STATUS_OK &&
      status != SYNC_DATABASE_ERROR_NOT_FOUND)
    return status;

  status = GetSyncRootDirectory(&contents->sync_root_directory_resource_id);
  if (status != SYNC_STATUS_OK &&
      status != SYNC_DATABASE_ERROR_NOT_FOUND)
    return status;

  return SYNC_STATUS_OK;
}

SyncStatusCode DriveMetadataDB::MigrateDatabaseIfNeeded() {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->Seek(kDatabaseVersionKey);

  int64 database_version = 0;
  if (itr->Valid() && itr->key().ToString() == kDatabaseVersionKey) {
    bool success = base::StringToInt64(itr->value().ToString(),
                                       &database_version);
    if (!success)
      return SYNC_DATABASE_ERROR_FAILED;
    if (database_version > kCurrentDatabaseVersion)
      return SYNC_DATABASE_ERROR_FAILED;
    if (database_version == kCurrentDatabaseVersion)
      return SYNC_STATUS_OK;
  }

  switch (database_version) {
    case 0:
      drive::MigrateDatabaseFromV0ToV1(db_.get());
      // fall-through
    case 1:
      drive::MigrateDatabaseFromV1ToV2(db_.get());
      return SYNC_STATUS_OK;
  }
  return SYNC_DATABASE_ERROR_FAILED;
}

SyncStatusCode DriveMetadataDB::SetLargestChangestamp(
    int64 largest_changestamp) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Put(kChangeStampKey, base::Int64ToString(largest_changestamp));
  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::SetSyncRootDirectory(
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Put(kSyncRootDirectoryKey, drive::RemoveWapiIdPrefix(resource_id));
  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::SetOriginRootDirectory(
    const GURL& origin,
    OriginSyncType sync_type,
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  std::string key = CreateKeyForOriginRoot(origin, sync_type);
  if (key.empty())
    return SYNC_DATABASE_ERROR_FAILED;

  leveldb::WriteBatch batch;
  batch.Put(key, drive::RemoveWapiIdPrefix(resource_id));
  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::GetSyncRootDirectory(std::string* resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Get(
      leveldb::ReadOptions(), kSyncRootDirectoryKey, resource_id);

  if (!IsDriveAPIEnabled() && status.ok())
    *resource_id = drive::AddWapiFolderPrefix(*resource_id);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateEntry(const FileSystemURL& url,
                                            DriveMetadata metadata) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  if (!IsDriveAPIEnabled()) {
    metadata.set_resource_id(
        drive::RemoveWapiIdPrefix(metadata.resource_id()));
  }

  std::string metadata_key = FileSystemURLToMetadataKey(url);
  std::string value;
  bool success = metadata.SerializeToString(&value);
  DCHECK(success);

  leveldb::WriteBatch batch;
  batch.Put(metadata_key, value);
  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::UpdateOriginAsIncrementalSync(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForOriginRoot(origin, DISABLED_ORIGIN));
  batch.Put(CreateKeyForOriginRoot(origin, INCREMENTAL_SYNC_ORIGIN),
            drive::RemoveWapiIdPrefix(resource_id));
  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::EnableOrigin(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  // No DB entry as enabled origins go back to
  // DriveFileSyncService::pending_batch_sync_origins only.
  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForOriginRoot(origin, INCREMENTAL_SYNC_ORIGIN));
  batch.Delete(CreateKeyForOriginRoot(origin, DISABLED_ORIGIN));

  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::DisableOrigin(
    const GURL& origin_to_disable, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForOriginRoot(origin_to_disable,
                                      INCREMENTAL_SYNC_ORIGIN));
  batch.Put(CreateKeyForOriginRoot(origin_to_disable, DISABLED_ORIGIN),
            drive::RemoveWapiIdPrefix(resource_id));

  // Remove entries specified by |origin_to_disable|.
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  std::string metadata_key = kDriveMetadataKeyPrefix + origin_to_disable.spec();
  for (itr->Seek(metadata_key); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveMetadataKeyPrefix, true))
      break;
    GURL origin;
    base::FilePath path;
    MetadataKeyToOriginAndPath(key, &origin, &path);
    if (origin != origin_to_disable)
      break;
    batch.Delete(key);
  }

  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::RemoveOrigin(const GURL& origin_to_remove) {
  DCHECK(CalledOnValidThread());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForOriginRoot(origin_to_remove,
                                      INCREMENTAL_SYNC_ORIGIN));
  batch.Delete(CreateKeyForOriginRoot(origin_to_remove, DISABLED_ORIGIN));

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  std::string metadata_key = kDriveMetadataKeyPrefix + origin_to_remove.spec();
  for (itr->Seek(metadata_key); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveMetadataKeyPrefix, true))
      break;
    GURL origin;
    base::FilePath path;
    MetadataKeyToOriginAndPath(key, &origin, &path);
    if (origin != origin_to_remove)
      break;
    batch.Delete(key);
  }

  return WriteToDB(&batch);
}

SyncStatusCode DriveMetadataDB::GetOrigins(
    ResourceIdByOrigin* incremental_sync_origins,
    ResourceIdByOrigin* disabled_origins) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));

  // Get incremental sync origins from the DB.
  for (itr->Seek(kDriveIncrementalSyncOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveIncrementalSyncOriginKeyPrefix, true))
      break;
    GURL origin(RemovePrefix(key, kDriveIncrementalSyncOriginKeyPrefix));
    DCHECK(origin.is_valid());

    std::string origin_resource_id = IsDriveAPIEnabled()
        ? itr->value().ToString()
        : drive::AddWapiFolderPrefix(itr->value().ToString());

    bool result = incremental_sync_origins->insert(
        std::make_pair(origin, origin_resource_id)).second;
    DCHECK(result);
  }

  // Get disabled origins from the DB.
  for (itr->Seek(kDriveDisabledOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveDisabledOriginKeyPrefix, true))
      break;
    GURL origin(RemovePrefix(key, kDriveDisabledOriginKeyPrefix));
    DCHECK(origin.is_valid());

    std::string origin_resource_id = IsDriveAPIEnabled()
        ? itr->value().ToString()
        : drive::AddWapiFolderPrefix(itr->value().ToString());

    bool result = disabled_origins->insert(
        std::make_pair(origin, origin_resource_id)).second;
    DCHECK(result);
  }

  return SYNC_STATUS_OK;
}

}  // namespace sync_file_system
