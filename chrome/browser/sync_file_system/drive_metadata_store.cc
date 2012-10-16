// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
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
const size_t kDriveMetadataKeyPrefixLength = arraysize(kDriveMetadataKeyPrefix);
}

namespace sync_file_system {

class DriveMetadataDB {
 public:
  typedef DriveMetadataStore::MetadataMap MetadataMap;

  DriveMetadataDB(const FilePath& base_dir,
                  base::SequencedTaskRunner* task_runner);
  ~DriveMetadataDB();

  SyncStatusCode Initialize();
  SyncStatusCode ReadContents(int64* largest_changestamp,
                              MetadataMap* metadata_map);

  SyncStatusCode SetLargestChangestamp(int64 largest_changestamp);
  SyncStatusCode UpdateEntry(const FileSystemURL& url,
                             const DriveMetadata& metadata);
  SyncStatusCode DeleteEntry(const FileSystemURL& url);

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
    DriveMetadataStore::MetadataMap* metadata_map) {
  DCHECK(db);
  DCHECK(largest_changestamp);
  DCHECK(metadata_map);

  *largest_changestamp = 0;
  metadata_map->clear();

  SyncStatusCode status = db->Initialize();
  if (status != fileapi::SYNC_STATUS_OK)
    return status;
  return db->ReadContents(largest_changestamp, metadata_map);
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
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE,
      base::Bind(InitializeDBOnFileThread,
                 db_.get(), largest_changestamp, metadata_map),
      base::Bind(&DriveMetadataStore::DidInitialize, AsWeakPtr(),
                 callback,
                 base::Owned(largest_changestamp),
                 base::Owned(metadata_map)));
}

void DriveMetadataStore::DidInitialize(const InitializationCallback& callback,
                                       const int64* largest_changestamp,
                                       MetadataMap* metadata_map,
                                       SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  DCHECK(largest_changestamp);
  DCHECK(metadata_map);

  if (status != fileapi::SYNC_STATUS_OK) {
    callback.Run(status, false);
    return;
  }

  db_status_ = status;
  largest_changestamp_ = *largest_changestamp;
  metadata_map_.swap(*metadata_map);
  // |largest_changestamp_| is set to 0 for a fresh empty database.
  callback.Run(status, largest_changestamp_ <= 0);
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

void DriveMetadataStore::UpdateDBStatus(SyncStatusCode status) {
  DCHECK(CalledOnValidThread());
  if (db_status_ == fileapi::SYNC_STATUS_OK)
    db_status_ = status;
  else
    LOG(WARNING) << "DriveMetadataStore turned to wrong state: " << status;
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

SyncStatusCode DriveMetadataDB::ReadContents(int64* largest_changestamp,
                                             MetadataMap* metadata_map) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_.get());
  DCHECK(largest_changestamp);
  DCHECK(metadata_map);

  *largest_changestamp = 0;
  metadata_map->clear();

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

  return fileapi::SYNC_STATUS_OK;
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

}  // namespace sync_file_system
