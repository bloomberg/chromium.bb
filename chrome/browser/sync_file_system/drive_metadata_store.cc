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

namespace {
const char* const kServiceName = DriveFileSyncService::kServiceName;
const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("DriveMetadata");
const char kChangeStampKey[] = "CHANGE_STAMP";
const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
const char kDriveMetadataKeyPrefix[] = "METADATA: ";
const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
const size_t kDriveMetadataKeyPrefixLength = arraysize(kDriveMetadataKeyPrefix);
}

class DriveMetadataDB {
 public:
  typedef DriveMetadataStore::MetadataMap MetadataMap;
  typedef DriveMetadataStore::ResourceIDMap ResourceIDMap;

  DriveMetadataDB(const base::FilePath& base_dir,
                  base::SequencedTaskRunner* task_runner);
  ~DriveMetadataDB();

  SyncStatusCode Initialize();
  SyncStatusCode ReadContents(DriveMetadataDBContents* contents);

  SyncStatusCode SetLargestChangestamp(int64 largest_changestamp);
  SyncStatusCode SetSyncRootDirectory(const std::string& resource_id);
  SyncStatusCode GetSyncRootDirectory(std::string* resource_id);
  SyncStatusCode UpdateEntry(const FileSystemURL& url,
                             const DriveMetadata& metadata);
  SyncStatusCode DeleteEntry(const FileSystemURL& url);

  SyncStatusCode UpdateSyncOriginAsBatch(const GURL& origin,
                                         const std::string& resource_id);
  SyncStatusCode UpdateSyncOriginAsIncremental(const GURL& origin,
                                               const std::string& resource_id);
  SyncStatusCode RemoveOrigin(const GURL& origin);

  SyncStatusCode GetSyncOrigins(ResourceIDMap* batch_sync_origins,
                                ResourceIDMap* incremental_sync_origins);

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
  DriveMetadataStore::ResourceIDMap batch_sync_origins;
  DriveMetadataStore::ResourceIDMap incremental_sync_origins;
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

  SyncStatusCode status = db->Initialize();
  if (status != SYNC_STATUS_OK)
    return status;
  return db->ReadContents(contents);
}

// Returns a key string for the given batch sync origin.
// For example, when |origin| is "http://www.example.com",
// returns "BSYNC_ORIGIN: http://www.example.com".
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

void AddOriginsToVector(std::vector<GURL>* all_origins,
                        const DriveMetadataStore::ResourceIDMap& resource_map) {
  typedef DriveMetadataStore::ResourceIDMap::const_iterator itr;
  for (std::map<GURL, std::string>::const_iterator itr = resource_map.begin();
       itr != resource_map.end();
       ++itr) {
    all_origins->push_back(itr->first);
  }
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
  // |largest_changestamp_| is set to 0 for a fresh empty database.
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

void DriveMetadataStore::RestoreSyncOrigins(
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  ResourceIDMap* batch_sync_origins = new ResourceIDMap;
  ResourceIDMap* incremental_sync_origins = new ResourceIDMap;
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::GetSyncOrigins,
                 base::Unretained(db_.get()),
                 batch_sync_origins,
                 incremental_sync_origins),
      base::Bind(&DriveMetadataStore::DidRestoreSyncOrigins,
                 AsWeakPtr(), callback,
                 base::Owned(batch_sync_origins),
                 base::Owned(incremental_sync_origins)));
}

void DriveMetadataStore::DidRestoreSyncOrigins(
    const SyncStatusCallback& callback,
    ResourceIDMap* batch_sync_origins,
    ResourceIDMap* incremental_sync_origins,
    SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(batch_sync_origins);
  DCHECK(incremental_sync_origins);

  db_status_ = status;
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  batch_sync_origins_.swap(*batch_sync_origins);
  incremental_sync_origins_.swap(*incremental_sync_origins);
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
  DCHECK(!metadata.resource_id().empty());
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
  DCHECK(!resource_id.empty());
  DCHECK(sync_root_directory_resource_id_.empty());

  sync_root_directory_resource_id_ = resource_id;

  // Set the resource ID for the sync root directory in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::SetSyncRootDirectory,
                 base::Unretained(db_.get()), resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

bool DriveMetadataStore::IsBatchSyncOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(batch_sync_origins_, origin);
}

bool DriveMetadataStore::IsIncrementalSyncOrigin(const GURL& origin) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(incremental_sync_origins_, origin);
}

void DriveMetadataStore::AddBatchSyncOrigin(const GURL& origin,
                                            const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsBatchSyncOrigin(origin));
  DCHECK(!IsIncrementalSyncOrigin(origin));
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  batch_sync_origins_.insert(std::make_pair(origin, resource_id));

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateSyncOriginAsBatch,
                 base::Unretained(db_.get()), origin, resource_id),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

void DriveMetadataStore::MoveBatchSyncOriginToIncremental(const GURL& origin) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsBatchSyncOrigin(origin));
  DCHECK(!IsIncrementalSyncOrigin(origin));
  DCHECK_EQ(SYNC_STATUS_OK, db_status_);

  std::map<GURL, std::string>::iterator found =
      batch_sync_origins_.find(origin);
  incremental_sync_origins_.insert(std::make_pair(origin, found->second));

  // Store a pair of |origin| and |resource_id| in the DB.
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateSyncOriginAsIncremental,
                 base::Unretained(db_.get()), origin, found->second),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));

  batch_sync_origins_.erase(found);
}

void DriveMetadataStore::RemoveOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(CalledOnValidThread());

  metadata_map_.erase(origin);
  batch_sync_origins_.erase(origin);
  incremental_sync_origins_.erase(origin);

  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::RemoveOrigin,
                 base::Unretained(db_.get()), origin),
      base::Bind(&DriveMetadataStore::DidRemoveOrigin, AsWeakPtr(), callback));
}

void DriveMetadataStore::DidRemoveOrigin(
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
        urls->insert(fileapi::CreateSyncableFileSystemURL(
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
        fileapi::FileSystemURL url = fileapi::CreateSyncableFileSystemURL(
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
  DCHECK(IsBatchSyncOrigin(origin) || IsIncrementalSyncOrigin(origin));

  ResourceIDMap::const_iterator found = incremental_sync_origins_.find(origin);
  if (found != incremental_sync_origins_.end())
    return found->second;

  found = batch_sync_origins_.find(origin);
  if (found != batch_sync_origins_.end())
    return found->second;

  NOTREACHED();
  return std::string();
}

void DriveMetadataStore::GetAllOrigins(std::vector<GURL>* origins) {
  origins->reserve(batch_sync_origins_.size() +
                   incremental_sync_origins_.size());
  AddOriginsToVector(origins, batch_sync_origins_);
  AddOriginsToVector(origins, incremental_sync_origins_);
}

////////////////////////////////////////////////////////////////////////////////

DriveMetadataDB::DriveMetadataDB(const base::FilePath& base_dir,
                                 base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner),
      db_path_(fileapi::FilePathToString(base_dir.Append(kDatabaseName))) {
}

DriveMetadataDB::~DriveMetadataDB() {
  DCHECK(CalledOnValidThread());
}

SyncStatusCode DriveMetadataDB::Initialize() {
  DCHECK(CalledOnValidThread());
  DCHECK(!db_.get());

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, db_path_, &db);
  // TODO(tzik): Handle database corruption. http://crbug.com/153709
  if (!status.ok())
    return LevelDBStatusToSyncStatusCode(status);
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
      std::string url_string(
          key.begin() + kDriveMetadataKeyPrefixLength - 1, key.end());
      FileSystemURL url;
      bool success = fileapi::DeserializeSyncableFileSystemURL(
          url_string, &url);
      DCHECK(success);

      DriveMetadata metadata;
      success = metadata.ParseFromString(itr->value().ToString());
      DCHECK(success);

      success = contents->metadata_map[url.origin()].insert(
          std::make_pair(url.path(), metadata)).second;
      DCHECK(success);
    }
  }

  SyncStatusCode status = GetSyncOrigins(&contents->batch_sync_origins,
                                         &contents->incremental_sync_origins);
  if (status != SYNC_STATUS_OK &&
      status != SYNC_DATABASE_ERROR_NOT_FOUND)
    return status;

  status = GetSyncRootDirectory(&contents->sync_root_directory_resource_id);
  if (status != SYNC_STATUS_OK &&
      status != SYNC_DATABASE_ERROR_NOT_FOUND)
    return status;

  return SYNC_STATUS_OK;
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

  std::string url_string;
  bool success = fileapi::SerializeSyncableFileSystemURL(url, &url_string);
  DCHECK(success);

  std::string value;
  success = metadata.SerializeToString(&value);
  DCHECK(success);
  leveldb::Status status  = db_->Put(
      leveldb::WriteOptions(),
      kDriveMetadataKeyPrefix + url_string,
      value);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::DeleteEntry(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  std::string url_string;
  bool success = fileapi::SerializeSyncableFileSystemURL(url, &url_string);
  DCHECK(success);

  leveldb::Status status = db_->Delete(
      leveldb::WriteOptions(),
      kDriveMetadataKeyPrefix + url_string);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateSyncOriginAsBatch(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(),
      CreateKeyForBatchSyncOrigin(origin),
      resource_id);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateSyncOriginAsIncremental(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForBatchSyncOrigin(origin));
  batch.Put(CreateKeyForIncrementalSyncOrigin(origin), resource_id);
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);

  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::RemoveOrigin(const GURL& origin) {
  DCHECK(CalledOnValidThread());

  leveldb::WriteBatch batch;
  batch.Delete(kDriveBatchSyncOriginKeyPrefix + origin.spec());
  batch.Delete(kDriveIncrementalSyncOriginKeyPrefix + origin.spec());

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  std::string serialized_origin;
  bool success = fileapi::SerializeSyncableFileSystemURL(
      fileapi::CreateSyncableFileSystemURL(
          origin, kServiceName, base::FilePath()), &serialized_origin);
  DCHECK(success);
  for (itr->Seek(kDriveMetadataKeyPrefix + serialized_origin);
       itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveMetadataKeyPrefix, true))
      break;
    std::string serialized_url(key.begin() + kDriveMetadataKeyPrefixLength - 1,
                               key.end());
    FileSystemURL url;
    bool success = fileapi::DeserializeSyncableFileSystemURL(
        serialized_url, &url);
    DCHECK(success);
    if (url.origin() != origin)
      break;
    batch.Delete(key);
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  return LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::GetSyncOrigins(
    ResourceIDMap* batch_sync_origins,
    ResourceIDMap* incremental_sync_origins) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));

  // Get batch sync origins from the DB.
  for (itr->Seek(kDriveBatchSyncOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    const std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveBatchSyncOriginKeyPrefix, true))
      break;
    const GURL origin(std::string(
        key.begin() + arraysize(kDriveBatchSyncOriginKeyPrefix) - 1,
        key.end()));
    DCHECK(origin.is_valid());
    const bool result = batch_sync_origins->insert(
        std::make_pair(origin, itr->value().ToString())).second;
    DCHECK(result);
  }

  // Get incremental sync origins from the DB.
  for (itr->Seek(kDriveIncrementalSyncOriginKeyPrefix);
       itr->Valid(); itr->Next()) {
    const std::string key = itr->key().ToString();
    if (!StartsWithASCII(key, kDriveIncrementalSyncOriginKeyPrefix, true))
      break;
    const GURL origin(std::string(
        key.begin() + arraysize(kDriveIncrementalSyncOriginKeyPrefix) - 1,
        key.end()));
    DCHECK(origin.is_valid());
    const bool result = incremental_sync_origins->insert(
        std::make_pair(origin, itr->value().ToString())).second;
    DCHECK(result);
  }

  return SYNC_STATUS_OK;
}

}  // namespace sync_file_system
