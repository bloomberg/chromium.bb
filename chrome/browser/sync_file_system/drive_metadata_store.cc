// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "googleurl/src/gurl.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemURL;
using fileapi::SyncStatusCode;

namespace {
const FilePath::CharType kDatabaseName[] = FILE_PATH_LITERAL("DriveMetadata");
const char kChangeStampKey[] = "CHANGE_STAMP";
const char kDriveMetadataKeyPrefix[] = "METADATA: ";
const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";
const size_t kDriveMetadataKeyPrefixLength = arraysize(kDriveMetadataKeyPrefix);
}

namespace sync_file_system {

class DriveMetadataDB {
 public:
  typedef DriveMetadataStore::MetadataMap MetadataMap;
  typedef DriveMetadataStore::ResourceIDMap ResourceIDMap;

  DriveMetadataDB(const FilePath& base_dir,
                  base::SequencedTaskRunner* task_runner);
  ~DriveMetadataDB();

  SyncStatusCode Initialize();
  SyncStatusCode ReadContents(int64* largest_changestamp,
                              MetadataMap* metadata_map,
                              ResourceIDMap* batch_sync_origins,
                              ResourceIDMap* incremental_sync_origins);

  SyncStatusCode SetLargestChangestamp(int64 largest_changestamp);
  SyncStatusCode UpdateEntry(const FileSystemURL& url,
                             const DriveMetadata& metadata);
  SyncStatusCode DeleteEntry(const FileSystemURL& url);

  SyncStatusCode UpdateSyncOriginAsBatch(const GURL& origin,
                                         const std::string& resource_id);
  SyncStatusCode UpdateSyncOriginAsIncremental(const GURL& origin,
                                               const std::string& resource_id);

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

namespace {

SyncStatusCode InitializeDBOnFileThread(
    DriveMetadataDB* db,
    int64* largest_changestamp,
    DriveMetadataStore::MetadataMap* metadata_map,
    DriveMetadataStore::ResourceIDMap* batch_sync_origins,
    DriveMetadataStore::ResourceIDMap* incremental_sync_origins) {
  DCHECK(db);
  DCHECK(largest_changestamp);
  DCHECK(metadata_map);
  DCHECK(batch_sync_origins);
  DCHECK(incremental_sync_origins);

  *largest_changestamp = 0;
  metadata_map->clear();
  batch_sync_origins->clear();
  incremental_sync_origins->clear();

  SyncStatusCode status = db->Initialize();
  if (status != fileapi::SYNC_STATUS_OK)
    return status;
  return db->ReadContents(largest_changestamp, metadata_map,
                          batch_sync_origins, incremental_sync_origins);
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

}  // namespace

DriveMetadataStore::DriveMetadataStore(
    const FilePath& base_dir,
    base::SequencedTaskRunner* file_task_runner)
    : file_task_runner_(file_task_runner),
      db_(new DriveMetadataDB(base_dir, file_task_runner)),
      db_status_(fileapi::SYNC_STATUS_UNKNOWN),
      largest_changestamp_(0) {
  DCHECK(file_task_runner);
}

DriveMetadataStore::~DriveMetadataStore() {
  DCHECK(CalledOnValidThread());
  file_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

void DriveMetadataStore::Initialize(const InitializationCallback& callback) {
  DCHECK(CalledOnValidThread());
  int64* largest_changestamp = new int64;
  MetadataMap* metadata_map = new MetadataMap;
  ResourceIDMap* batch_sync_origins = new ResourceIDMap;
  ResourceIDMap* incremental_sync_origins = new ResourceIDMap;

  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(InitializeDBOnFileThread,
                 db_.get(), largest_changestamp, metadata_map,
                 batch_sync_origins, incremental_sync_origins),
      base::Bind(&DriveMetadataStore::DidInitialize, AsWeakPtr(),
                 callback,
                 base::Owned(largest_changestamp),
                 base::Owned(metadata_map),
                 base::Owned(batch_sync_origins),
                 base::Owned(incremental_sync_origins)));
}

void DriveMetadataStore::DidInitialize(const InitializationCallback& callback,
                                       const int64* largest_changestamp,
                                       MetadataMap* metadata_map,
                                       ResourceIDMap* batch_sync_origins,
                                       ResourceIDMap* incremental_sync_origins,
                                       SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(largest_changestamp);
  DCHECK(metadata_map);
  DCHECK(batch_sync_origins);
  DCHECK(incremental_sync_origins);

  db_status_ = status;
  if (status != fileapi::SYNC_STATUS_OK) {
    callback.Run(status, false);
    return;
  }

  largest_changestamp_ = *largest_changestamp;
  metadata_map_.swap(*metadata_map);
  batch_sync_origins_.swap(*batch_sync_origins);
  incremental_sync_origins_.swap(*incremental_sync_origins);
  // |largest_changestamp_| is set to 0 for a fresh empty database.
  callback.Run(status, largest_changestamp_ <= 0);
}

void DriveMetadataStore::RestoreSyncOrigins(
    const RestoreSyncOriginsCallback& callback) {
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
    const RestoreSyncOriginsCallback& callback,
    ResourceIDMap* batch_sync_origins,
    ResourceIDMap* incremental_sync_origins,
    SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(batch_sync_origins);
  DCHECK(incremental_sync_origins);

  db_status_ = status;
  if (status != fileapi::SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  batch_sync_origins_.swap(*batch_sync_origins);
  incremental_sync_origins_.swap(*incremental_sync_origins);
  callback.Run(status);
}

void DriveMetadataStore::SetLargestChangeStamp(int64 largest_changestamp) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, db_status_);
  largest_changestamp_ = largest_changestamp;
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::SetLargestChangestamp,
                 base::Unretained(db_.get()), largest_changestamp),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
}

int64 DriveMetadataStore::GetLargestChangeStamp() const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, db_status_);
  return largest_changestamp_;
}

SyncStatusCode DriveMetadataStore::UpdateEntry(const FileSystemURL& url,
                                               const DriveMetadata& metadata) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, db_status_);
  std::pair<MetadataMap::iterator, bool> result =
      metadata_map_.insert(std::make_pair(url, metadata));
  if (!result.second)
    result.first->second = metadata;
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(&DriveMetadataDB::UpdateEntry, base::Unretained(db_.get()),
                 url, metadata),
      base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
  return result.second ?
      fileapi::SYNC_STATUS_OK :
      fileapi::SYNC_FILE_ERROR_EXISTS;
}

SyncStatusCode DriveMetadataStore::DeleteEntry(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  if (metadata_map_.erase(url) == 1) {
    base::PostTaskAndReplyWithResult(
        file_task_runner_, FROM_HERE,
        base::Bind(&DriveMetadataDB::DeleteEntry,
                   base::Unretained(db_.get()), url),
        base::Bind(&DriveMetadataStore::UpdateDBStatus, AsWeakPtr()));
    return fileapi::SYNC_STATUS_OK;
  }
  return fileapi::SYNC_DATABASE_ERROR_NOT_FOUND;
}

SyncStatusCode DriveMetadataStore::ReadEntry(const FileSystemURL& url,
                                             DriveMetadata* metadata) const {
  DCHECK(CalledOnValidThread());
  DCHECK(metadata);

  MetadataMap::const_iterator itr = metadata_map_.find(url);
  if (itr == metadata_map_.end())
    return fileapi::SYNC_DATABASE_ERROR_NOT_FOUND;
  *metadata = itr->second;
  return fileapi::SYNC_STATUS_OK;
}

void DriveMetadataStore::SetSyncRootDirectory(const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(!resource_id.empty());
  DCHECK(sync_root_directory_resource_id_.empty());

  // TODO(tzik): Store |sync_root_directory_resource_id_| to DB.
  // crbug.com/158029
  sync_root_directory_resource_id_ = resource_id;
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
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, db_status_);

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
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, db_status_);

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

void DriveMetadataStore::UpdateDBStatus(SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  db_status_ = status;
  if (db_status_ != fileapi::SYNC_STATUS_OK) {
    // TODO(tzik): Handle database corruption. http://crbug.com/153709
    LOG(WARNING) << "DriveMetadataStore turned to wrong state: " << status;
  }
}

////////////////////////////////////////////////////////////////////////////////

DriveMetadataDB::DriveMetadataDB(const FilePath& base_dir,
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
    return fileapi::LevelDBStatusToSyncStatusCode(status);
  db_.reset(db);
  return fileapi::SYNC_STATUS_OK;
}

SyncStatusCode DriveMetadataDB::ReadContents(
    int64* largest_changestamp,
    MetadataMap* metadata_map,
    ResourceIDMap* batch_sync_origins,
    ResourceIDMap* incremental_sync_origins) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());
  DCHECK(largest_changestamp);
  DCHECK(metadata_map);
  DCHECK(batch_sync_origins);
  DCHECK(incremental_sync_origins);

  *largest_changestamp = 0;
  metadata_map->clear();
  batch_sync_origins->clear();
  incremental_sync_origins->clear();

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (key == kChangeStampKey) {
      bool success = base::StringToInt64(itr->value().ToString(),
                                         largest_changestamp);
      DCHECK(success);
    } else if (StartsWithASCII(key, kDriveMetadataKeyPrefix, true)) {
      std::string url_string(
          key.begin() + kDriveMetadataKeyPrefixLength - 1, key.end());
      fileapi::FileSystemURL url;
      bool success = fileapi::DeserializeSyncableFileSystemURL(
          url_string, &url);
      DCHECK(success);

      DriveMetadata metadata;
      success = metadata.ParseFromString(itr->value().ToString());
      DCHECK(success);

      success = metadata_map->insert(std::make_pair(url, metadata)).second;
      DCHECK(success);
    }
  }

  return GetSyncOrigins(batch_sync_origins, incremental_sync_origins);
}

SyncStatusCode DriveMetadataDB::SetLargestChangestamp(
    int64 largest_changestamp) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(),
      kChangeStampKey, base::Int64ToString(largest_changestamp));
  return fileapi::LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateEntry(const fileapi::FileSystemURL& url,
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

  return fileapi::LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::DeleteEntry(const fileapi::FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  std::string url_string;
  bool success = fileapi::SerializeSyncableFileSystemURL(url, &url_string);
  DCHECK(success);

  leveldb::Status status = db_->Delete(
      leveldb::WriteOptions(),
      kDriveMetadataKeyPrefix + url_string);
  return fileapi::LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateSyncOriginAsBatch(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(),
      CreateKeyForBatchSyncOrigin(origin),
      resource_id);
  return fileapi::LevelDBStatusToSyncStatusCode(status);
}

SyncStatusCode DriveMetadataDB::UpdateSyncOriginAsIncremental(
    const GURL& origin, const std::string& resource_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(CreateKeyForBatchSyncOrigin(origin));
  batch.Put(CreateKeyForIncrementalSyncOrigin(origin), resource_id);
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);

  return fileapi::LevelDBStatusToSyncStatusCode(status);
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

  return fileapi::SYNC_STATUS_OK;
}

}  // namespace sync_file_system
