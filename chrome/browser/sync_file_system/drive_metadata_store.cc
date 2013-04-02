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
#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "googleurl/src/gurl.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

typedef DriveMetadataStore::ResourceIdByOrigin ResourceIdByOrigin;
typedef DriveMetadataStore::OriginByResourceId OriginByResourceId;

const base::FilePath::CharType DriveMetadataStore::kDatabaseName[] =
    FILE_PATH_LITERAL("DriveMetadata");

namespace {

const char* const kServiceName = DriveFileSyncService::kServiceName;
const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 1;
const char kChangeStampKey[] = "CHANGE_STAMP";
const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
const char kDriveMetadataKeyPrefix[] = "METADATA: ";
const char kMetadataKeySeparator = ' ';
const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
const char kDriveDisabledOriginKeyPrefix[] = "DISABLED_ORIGIN: ";
const size_t kDriveMetadataKeyPrefixLength = arraysize(kDriveMetadataKeyPrefix);

const base::FilePath::CharType kV0FormatPathPrefix[] =
    FILE_PATH_LITERAL("drive/");

bool ParseV0FormatFileSystemURLString(const GURL& url,
                                      GURL* origin,
                                      base::FilePath* path) {
  fileapi::FileSystemType mount_type;
  base::FilePath virtual_path;

  if (!fileapi::FileSystemURL::ParseFileSystemSchemeURL(
          url, origin, &mount_type, &virtual_path) ||
      mount_type != fileapi::kFileSystemTypeExternal) {
    NOTREACHED() << "Failed to parse filesystem scheme URL";
    return false;
  }

  base::FilePath::StringType prefix =
      base::FilePath(kV0FormatPathPrefix).NormalizePathSeparators().value();
  if (virtual_path.value().substr(0, prefix.size()) != prefix)
    return false;

  *path = base::FilePath(virtual_path.value().substr(prefix.size()));
  return true;
}

std::string FileSystemURLToMetadataKey(const FileSystemURL& url) {
  return kDriveMetadataKeyPrefix + url.origin().spec() +
      kMetadataKeySeparator + url.path().AsUTF8Unsafe();
}

void MetadataKeyToOriginAndPath(const std::string& metadata_key,
                                GURL* origin,
                                base::FilePath* path) {
  std::string key_body(metadata_key.begin() + kDriveMetadataKeyPrefixLength - 1,
                       metadata_key.end());
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
    BATCH_SYNC_ORIGIN,
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
  SyncStatusCode MigrateFromVersion0Database();

  SyncStatusCode SetLargestChangestamp(int64 largest_changestamp);
  SyncStatusCode SetSyncRootDirectory(const std::string& resource_id);
  SyncStatusCode GetSyncRootDirectory(std::string* resource_id);
  SyncStatusCode SetOriginRootDirectory(const GURL& origin,
                                        OriginSyncType sync_type,
                                        const std::string& resource_id);
  SyncStatusCode UpdateEntry(const FileSystemURL& url,
                             const DriveMetadata& metadata);
  SyncStatusCode DeleteEntry(const FileSystemURL& url);

  // TODO(calvinlo): consolidate these state transition functions for sync
  // origins like "UpdateOrigin(GURL, SyncStatusEnum)". And manage origins in
  // just one map like "Map<SyncStatusEnum, ResourceIDMap>".
  // http://crbug.com/211600
  SyncStatusCode UpdateOriginAsBatchSync(const GURL& origin,
                                         const std::string& resource_id);
  SyncStatusCode UpdateOriginAsIncrementalSync(const GURL& origin,
                                               const std::string& resource_id);
  SyncStatusCode EnableOrigin(const GURL& origin,
                              const std::string& resource_id);
  SyncStatusCode DisableOrigin(const GURL& origin,
                               const std::string& resource_id);
  SyncStatusCode RemoveOrigin(const GURL& origin);

  SyncStatusCode GetOrigins(ResourceIdByOrigin* batch_sync_origins,
                            ResourceIdByOrigin* incremental_sync_origins,
                            ResourceIdByOrigin* disabled_origins);

 private:
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
  ResourceIdByOrigin batch_sync_origins;
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
  contents->batch_sync_origins.clear();
  contents->incremental_sync_origins.clear();
  contents->disabled_origins.clear();

  bool created = false;
  SyncStatusCode status = db->Initialize(&created);
  if (status != SYNC_STATUS_OK)
    return status;

  if (!created) {
    status = db->MigrateDatabaseIfNeeded();
    if (status != SYNC_STATUS_OK) {
      LOG(WARNING) << "Failed to migrate DriveMetadataStore to latest version.";
      return status;
    }
  }

  return db->ReadContents(contents);
}

std::string CreateKeyForOriginRoot(const GURL& origin,
                                   DriveMetadataDB::OriginSyncType sync_type) {
  DCHECK(origin.is_valid());
  switch (sync_type) {
    case DriveMetadataDB::BATCH_SYNC_ORIGIN:
      return kDriveBatchSyncOriginKeyPrefix + origin.spec();
    case DriveMetadataDB::INCREMENTAL_SYNC_ORIGIN:
      return kDriveIncrementalSyncOriginKeyPrefix + origin.spec();
    case DriveMetadataDB::DISABLED_ORIGIN:
      return kDriveDisabledOriginKeyPrefix + origin.spec();
  }
  NOTREACHED();
  return std::string();
}

// Returns a key string for the given batch sync origin.
// For example, when |origin| is "http://www.example.com",
// returns "BSYNC_ORIGIN: http://www.example.com".
// TODO(calvinlo): consolidate these CreateKeyFor* functions.
// http://crbug.com/211600
std::string CreateKeyForBatchSyncOrigin(const GURL& origin) {
  DCHECK(origin.is_valid());
  return kDriveBatchSyncOriginKeyPrefix + origin.spec();
}

// Returns a key string for the given incremental sync origin.
// For example, when |origin| is "http://www.example.com",
// returns "ISYNC_ORIGIN: http://www.example.com".
std::string CreateKeyForIncrementalSyncOrigin(const GURL& origin) {
  DCHECK(origin.is_valid());
  return kDriveIncrementalSyncOriginKeyPrefix + origin.spec();
}

// Returns a key string for the given disabled origin.
// For example, when |origin| is "http://www.example.com",
// returns "DISABLED_ORIGIN: http://www.example.com".
std::string CreateKeyForDisabledOrigin(const GURL& origin) {
  DCHECK(origin.is_valid());
  return kDriveDisabledOriginKeyPrefix + origin.spec();
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
      file_task_runner_, FROM_HERE,
      base::Bind(InitializeDBOnFileThread, db_.get(), contents),
      base::Bind(&DriveMetadataStore::DidInitialize, AsWeakPtr(),
                 callback, base::Owned(contents)));
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
  batch_sync_origins_.swap(contents->batch_sync_origins);
  incremental_sync_origins_.swap(contents->incremental_sync_origins);
  disabled_origins_.swap(contents->disabled_origins);
  // |largest_changestamp_| is set to 0 for a fresh empty database.

  origin_by_resource_id_.clear();
  InsertReverseMap(batch_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(incremental_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(disabled_origins_, &origin_by_resource_id_);

  callback.Run(status, largest_changestamp_ <= 0);
}

void DriveMetadataStore::RestoreSyncRootDirectory(
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string* sync_root_directory_resource_id = new std::string;
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::GetSyncRootDirectory,
                 base::Unretained(db_.get()),
                 sync_root_directory_resource_id),
      base::Bind(&DriveMetadataStore::DidRestoreSyncRootDirectory,
                 AsWeakPtr(), callback,
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
  ResourceIdByOrigin* batch_sync_origins = new ResourceIdByOrigin;
  ResourceIdByOrigin* incremental_sync_origins = new ResourceIdByOrigin;
  ResourceIdByOrigin* disabled_origins = new ResourceIdByOrigin;
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::GetOrigins,
                 base::Unretained(db_.get()),
                 batch_sync_origins,
                 incremental_sync_origins,
                 disabled_origins),
      base::Bind(&DriveMetadataStore::DidRestoreOrigins,
                 AsWeakPtr(), callback,
                 base::Owned(batch_sync_origins),
                 base::Owned(incremental_sync_origins),
                 base::Owned(disabled_origins)));
}

void DriveMetadataStore::DidRestoreOrigins(
    const SyncStatusCallback& callback,
    ResourceIdByOrigin* batch_sync_origins,
    ResourceIdByOrigin* incremental_sync_origins,
    ResourceIdByOrigin* disabled_origins,
    SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(batch_sync_origins);
  DCHECK(incremental_sync_origins);
  DCHECK(disabled_origins);

  db_status_ = status;
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  batch_sync_origins_.swap(*batch_sync_origins);
  incremental_sync_origins_.swap(*incremental_sync_origins);
  disabled_origins_.swap(*disabled_origins);

  origin_by_resource_id_.clear();
  InsertReverseMap(batch_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(incremental_sync_origins_, &origin_by_resource_id_);
  InsertReverseMap(disabled_origins_, &origin_by_resource_id_);

  callback.Run(status);
}

void DriveMetadataStore::SetLargestChangeStamp(
    int64 largest_changestamp,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);
  largest_changestamp_ = largest_changestamp;
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::SetLargestChangestamp,
                 base::Unretained(db_.get()), largest_changestamp),
      base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                 AsWeakPtr(), callback));
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
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateEntry, base::Unretained(db_.get()),
                 url, metadata),
      base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                 AsWeakPtr(), callback));
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
    base::PostTaskAndReplyWithResult(
        file_task_runner_, FROM_HERE,
        base::Bind(&DriveMetadataDB::DeleteEntry,
                   base::Unretained(db_.get()), url),
        base::Bind(&DriveMetadataStore::UpdateDBStatusAndInvokeCallback,
                   AsWeakPtr(), callback));
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

void DriveMetadataStore::SetSyncRootDirectory(const std::string& resource_id) {
  DCHECK(CalledOnValidThread());

  sync_root_directory_resource_id_ = resource_id;

  // Set the resource ID for the sync root directory in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::SetSyncRootDirectory,
                 base::Unretained(db_.get()), resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

void DriveMetadataStore::SetOriginRootDirectory(
    const GURL& origin,
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsKnownOrigin(origin));

  DriveMetadataDB::OriginSyncType sync_type;
  if (UpdateResourceIdMap(&batch_sync_origins_, &origin_by_resource_id_,
                          origin, resource_id)) {
    sync_type = DriveMetadataDB::BATCH_SYNC_ORIGIN;
  } else if (UpdateResourceIdMap(
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
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::SetOriginRootDirectory,
                 base::Unretained(db_.get()), origin, sync_type, resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

bool DriveMetadataStore::IsKnownOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return IsBatchSyncOrigin(origin) ||
      IsIncrementalSyncOrigin(origin) ||
      IsOriginDisabled(origin);
}

bool DriveMetadataStore::IsBatchSyncOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(batch_sync_origins_, origin);
}

bool DriveMetadataStore::IsIncrementalSyncOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(incremental_sync_origins_, origin);
}

bool DriveMetadataStore::IsOriginDisabled(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(disabled_origins_, origin);
}

void DriveMetadataStore::AddBatchSyncOrigin(const GURL& origin,
                                            const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsBatchSyncOrigin(origin));
  DCHECK(!IsIncrementalSyncOrigin(origin));
  DCHECK(!IsOriginDisabled(origin));
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  batch_sync_origins_.insert(std::make_pair(origin, resource_id));
  origin_by_resource_id_.insert(std::make_pair(resource_id, origin));

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateOriginAsBatchSync,
                 base::Unretained(db_.get()), origin, resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

void DriveMetadataStore::MoveBatchSyncOriginToIncremental(const GURL& origin) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsBatchSyncOrigin(origin));
  DCHECK(!IsIncrementalSyncOrigin(origin));
  DCHECK(!IsOriginDisabled(origin));
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  std::map<GURL, std::string>::iterator found =
      batch_sync_origins_.find(origin);
  incremental_sync_origins_.insert(std::make_pair(origin, found->second));

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateOriginAsIncrementalSync,
                 base::Unretained(db_.get()), origin, found->second),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));

  batch_sync_origins_.erase(found);
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

  // Ensure |origin| is marked as a batch sync origin.
  batch_sync_origins_.insert(std::make_pair(origin, resource_id));
  found = incremental_sync_origins_.find(origin);
  if (found != incremental_sync_origins_.end())
    incremental_sync_origins_.erase(found);

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::EnableOrigin,
                 base::Unretained(db_.get()), origin, resource_id),
      base::Bind(&DriveMetadataStore::DidUpdateOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string resource_id;
  std::map<GURL, std::string>::iterator found =
      batch_sync_origins_.find(origin);
  if (found != batch_sync_origins_.end()) {
    resource_id = found->second;
    batch_sync_origins_.erase(found);
  }

  found = incremental_sync_origins_.find(origin);
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
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::DisableOrigin,
                 base::Unretained(db_.get()), origin, resource_id),
      base::Bind(&DriveMetadataStore::DidUpdateOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::RemoveOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  metadata_map_.erase(origin);

  std::string resource_id;
  if (EraseIfExists(&batch_sync_origins_, origin, &resource_id) ||
      EraseIfExists(&incremental_sync_origins_, origin, &resource_id) ||
      EraseIfExists(&disabled_origins_, origin, &resource_id)) {
    origin_by_resource_id_.erase(resource_id);
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::RemoveOrigin,
                 base::Unretained(db_.get()), origin),
      base::Bind(&DriveMetadataStore::DidUpdateOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::DidUpdateOrigin(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  UpdateDBStatus(status);
  callback.Run(status);
}

void DriveMetadataStore::UpdateDBStatus(SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  if (db_status_ != SYNC_STATUS_OK &&
      db_status_ != SYNC_DATABASE_ERROR_NOT_FOUND) {
    // TODO(tzik): Handle database corruption. http://crbug.com/153709
    db_status_ = status;
    LOG(WARNING) << "DriveMetadataStore turned to wrong state: " << status;
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
            origin_itr->first, kServiceName, itr->first));
      }
    }
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode DriveMetadataStore::GetToBeFetchedFiles(
    URLAndResourceIdList* list) const {
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
            origin_itr->first, kServiceName, itr->first);
        list->push_back(std::make_pair(url, itr->second.resource_id()));
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

  found = batch_sync_origins_.find(origin);
  if (found != batch_sync_origins_.end())
    return found->second;

  found = disabled_origins_.find(origin);
  if (found != disabled_origins_.end())
    return found->second;

  return std::string();
}

void DriveMetadataStore::GetEnabledOrigins(std::vector<GURL>* origins) {
  DCHECK(CalledOnValidThread());
  origins->clear();
  origins->reserve(batch_sync_origins_.size() +
                   incremental_sync_origins_.size());
  AddOriginsToVector(origins, batch_sync_origins_);
  AddOriginsToVector(origins, incremental_sync_origins_);
}

void DriveMetadataStore::GetDisabledOrigins(std::vector<GURL>* origins) {
  DCHECK(CalledOnValidThread());
  origins->clear();
  origins->reserve(disabled_origins_.size());
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
  return SYNC_STATUS_OK;
}

SyncStatusCode DriveMetadataDB::ReadContents(
    DriveMetadataDBContents* contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());
  DCHECK(contents);

  contents->largest_changestamp = 0;
  contents->metadata_map.clear();
  contents->batch_sync_origins.clear();
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

      success = contents->metadata_map[origin].insert(
          std::make_pair(path, metadata)).second;
      DCHECK(success);
    }
  }

  SyncStatusCode status = GetOrigins(&contents->batch_sync_origins,
                                     &contents->incremental_sync_origins,
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

  if (database_version == 0) {
    MigrateFromVersion0Database();
    return SYNC_STATUS_OK;
  }
  return SYNC_DATABASE_ERROR_FAILED;
}

SyncStatusCode DriveMetadataDB::MigrateFromVersion0Database() {
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

  leveldb::WriteBatch write_batch;
  write_batch.Put(kDatabaseVersionKey,
                  base::Int64ToString(kCurrentDatabaseVersion));

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kDriveMetadataKeyPrefix); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveMetadataKeyPrefix, true))
      break;
    std::string serialized_url(
        key.begin() + kDriveMetadataKeyPrefixLength - 1, key.end());

    GURL origin;
    base::FilePath path;
    bool success = ParseV0FormatFileSystemURLString(
        GURL(serialized_url), &origin, &path);
    DCHECK(success) << serialized_url;
    std::string new_key = kDriveMetadataKeyPrefix + origin.spec() +
        kMetadataKeySeparator + path.AsUTF8Unsafe();

    write_batch.Put(new_key, itr->value());
    write_batch.Delete(key);
  }
  return LevelDBStatusToSyncStatusCode(
      db_->Write(leveldb::WriteOptions(), &write_batch));
}

SyncStatusCode DriveMetadataDB::SetLargestChangestamp(
    int64 largest_changestamp) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(),
      kChangeStampKey, base::Int64ToString(largest_changestamp));
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::SetSyncRootDirectory(
    const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(), kSyncRootDirectoryKey, resource_id);
  return LevelDBStatusToSyncStatusCode(status);
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

  leveldb::Status status = db_->Put(leveldb::WriteOptions(), key, resource_id);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::GetSyncRootDirectory(std::string* resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Get(
      leveldb::ReadOptions(), kSyncRootDirectoryKey, resource_id);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateEntry(const FileSystemURL& url,
                                            const DriveMetadata& metadata) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  std::string metadata_key = FileSystemURLToMetadataKey(url);
  std::string value;
  bool success = metadata.SerializeToString(&value);
  DCHECK(success);
  leveldb::Status status  = db_->Put(
      leveldb::WriteOptions(), metadata_key, value);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::DeleteEntry(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  std::string metadata_key = FileSystemURLToMetadataKey(url);
  leveldb::Status status = db_->Delete(
      leveldb::WriteOptions(), metadata_key);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateOriginAsBatchSync(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(),
      CreateKeyForBatchSyncOrigin(origin),
      resource_id);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateOriginAsIncrementalSync(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForBatchSyncOrigin(origin));
  batch.Delete(CreateKeyForDisabledOrigin(origin));
  batch.Put(CreateKeyForIncrementalSyncOrigin(origin), resource_id);
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::EnableOrigin(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForIncrementalSyncOrigin(origin));
  batch.Delete(CreateKeyForDisabledOrigin(origin));
  batch.Put(CreateKeyForBatchSyncOrigin(origin), resource_id);
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::DisableOrigin(
    const GURL& origin_to_disable, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForBatchSyncOrigin(origin_to_disable));
  batch.Delete(CreateKeyForIncrementalSyncOrigin(origin_to_disable));
  batch.Put(CreateKeyForDisabledOrigin(origin_to_disable), resource_id);

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
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::RemoveOrigin(const GURL& origin_to_remove) {
  DCHECK(CalledOnValidThread());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForBatchSyncOrigin(origin_to_remove));
  batch.Delete(CreateKeyForIncrementalSyncOrigin(origin_to_remove));
  batch.Delete(CreateKeyForDisabledOrigin(origin_to_remove));

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

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::GetOrigins(
    ResourceIdByOrigin* batch_sync_origins,
    ResourceIdByOrigin* incremental_sync_origins,
    ResourceIdByOrigin* disabled_origins) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));

  // Get batch sync origins from the DB.
  for (itr->Seek(kDriveBatchSyncOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveBatchSyncOriginKeyPrefix, true))
      break;
    GURL origin(std::string(
        key.begin() + arraysize(kDriveBatchSyncOriginKeyPrefix) - 1,
        key.end()));
    DCHECK(origin.is_valid());
    bool result = batch_sync_origins->insert(
        std::make_pair(origin, itr->value().ToString())).second;
    DCHECK(result);
  }

  // Get incremental sync origins from the DB.
  for (itr->Seek(kDriveIncrementalSyncOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveIncrementalSyncOriginKeyPrefix, true))
      break;
    GURL origin(std::string(
        key.begin() + arraysize(kDriveIncrementalSyncOriginKeyPrefix) - 1,
        key.end()));
    DCHECK(origin.is_valid());
    bool result = incremental_sync_origins->insert(
        std::make_pair(origin, itr->value().ToString())).second;
    DCHECK(result);
  }

  // Get disabled origins from the DB.
  for (itr->Seek(kDriveDisabledOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveDisabledOriginKeyPrefix, true))
      break;
    GURL origin(std::string(
        key.begin() + arraysize(kDriveDisabledOriginKeyPrefix) - 1,
        key.end()));
    DCHECK(origin.is_valid());
    bool result = disabled_origins->insert(
        std::make_pair(origin, itr->value().ToString())).second;
    DCHECK(result);
  }

  return SYNC_STATUS_OK;
}

}  // namespace sync_file_system
