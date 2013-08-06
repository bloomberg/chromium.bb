// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include <stack>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 3;
const char kServiceMetadataKey[] = "SERVICE";
const char kFileMetadataKeyPrefix[] = "FILE: ";

struct DatabaseContents {
  scoped_ptr<ServiceMetadata> service_metadata;
};

namespace {

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWithASCII(str, prefix, true))
    return str.substr(prefix.size());
  return str;
}

base::FilePath ReverseConcatPathComponents(
    const std::vector<base::FilePath>& components) {
  if (components.empty())
    return base::FilePath(FILE_PATH_LITERAL("/")).NormalizePathSeparators();

  size_t total_size = 0;
  typedef std::vector<base::FilePath> PathComponents;
  for (PathComponents::const_iterator itr = components.begin();
       itr != components.end(); ++itr)
    total_size += itr->value().size() + 1;

  base::FilePath::StringType result;
  result.reserve(total_size);
  for (PathComponents::const_reverse_iterator itr = components.rbegin();
       itr != components.rend(); ++itr) {
    result.append(1, base::FilePath::kSeparators[0]);
    result.append(itr->value());
  }

  return base::FilePath(result).NormalizePathSeparators();
}

void AdaptLevelDBStatusToSyncStatusCode(const SyncStatusCallback& callback,
                                        const leveldb::Status& status) {
  callback.Run(LevelDBStatusToSyncStatusCode(status));
}

// Returns true if |db| has no content.
bool IsDatabaseEmpty(leveldb::DB* db) {
  DCHECK(db);
  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

SyncStatusCode OpenDatabase(const base::FilePath& path,
                            scoped_ptr<leveldb::DB>* db_out,
                            bool* created) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db_out);
  DCHECK(created);

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status db_status =
      leveldb::DB::Open(options, path.AsUTF8Unsafe(), &db);
  SyncStatusCode status = LevelDBStatusToSyncStatusCode(db_status);
  if (status != SYNC_STATUS_OK) {
    delete db;
    return status;
  }

  *created = IsDatabaseEmpty(db);
  db_out->reset(db);
  return status;
}

SyncStatusCode MigrateDatabaseIfNeeded(leveldb::DB* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  std::string value;
  leveldb::Status status =
      db->Get(leveldb::ReadOptions(), kDatabaseVersionKey, &value);
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
      // TODO(tzik): Migrate database from version 2 to 3.
      //   * Add sync-root folder as active, dirty and needs_folder_listing
      //     folder.
      //   * Add app-root folders for each origins.  Each app-root folder for
      //     an enabled origin should be a active, dirty and
      //     needs_folder_listing folder.  And Each app-root folder for a
      //     disabled origin should be an inactive, dirty and
      //     non-needs_folder_listing folder.
      //   * Add a file metadata for each file in previous version.
      NOTIMPLEMENTED();
      return SYNC_DATABASE_ERROR_FAILED;
      // fall-through
    case 3:
      DCHECK_EQ(3, kCurrentDatabaseVersion);
      return SYNC_STATUS_OK;
    default:
      return SYNC_DATABASE_ERROR_FAILED;
  }
}

SyncStatusCode WriteVersionInfo(leveldb::DB* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  return LevelDBStatusToSyncStatusCode(
      db->Put(leveldb::WriteOptions(),
              kDatabaseVersionKey,
              base::Int64ToString(kCurrentDatabaseVersion)));
}

SyncStatusCode ReadDatabaseContents(leveldb::DB* db,
                                    DatabaseContents* contents) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  DCHECK(contents);

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    std::string value = itr->value().ToString();
    if (key == kServiceMetadataKey) {
      scoped_ptr<ServiceMetadata> service_metadata(new ServiceMetadata);
      if (!service_metadata->ParseFromString(value)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse SyncServiceMetadata");
        continue;
      }

      contents->service_metadata = service_metadata.Pass();
      continue;
    }

    NOTIMPLEMENTED();
    continue;
  }

  return SYNC_STATUS_OK;
}

SyncStatusCode InitializeServiceMetadata(DatabaseContents* contents,
                                         leveldb::WriteBatch* batch) {

  if (!contents->service_metadata) {
    contents->service_metadata.reset(new ServiceMetadata);

    std::string value;
    contents->service_metadata->SerializeToString(&value);
    batch->Put(kServiceMetadataKey, value);
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode RemoveUnreachableFiles(DatabaseContents* contents,
                                      leveldb::WriteBatch* batch) {
  NOTIMPLEMENTED();
  return SYNC_STATUS_OK;
}

template <typename Container, typename Key, typename Value>
bool FindItem(const Container& container, const Key& key, Value* value) {
  typename Container::const_iterator found = container.find(key);
  if (found == container.end())
    return false;
  if (value)
    *value = *found->second;
  return true;
}

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::MessageLoopProxy::current()->PostTask(from_here, closure);
}

}  // namespace

// static
void MetadataDatabase::Create(base::SequencedTaskRunner* task_runner,
                              const base::FilePath& database_path,
                              const CreateCallback& callback) {
  task_runner->PostTask(FROM_HERE, base::Bind(
      &CreateOnTaskRunner,
      base::MessageLoopProxy::current(),
      make_scoped_refptr(task_runner),
      database_path, callback));
}

MetadataDatabase::~MetadataDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

int64 MetadataDatabase::GetLargestChangeID() const {
  return service_metadata_->largest_change_id();
}

void MetadataDatabase::RegisterApp(const std::string& app_id,
                                   const std::string& folder_id,
                                   const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::DisableApp(const std::string& app_id,
                                  const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::EnableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::UnregisterApp(const std::string& app_id,
                                     const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::UpdateByChangeList(
    ScopedVector<google_apis::ChangeResource> changes,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

MetadataDatabase::MetadataDatabase(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  DCHECK(task_runner);
}

// static
void MetadataDatabase::CreateOnTaskRunner(
    base::SingleThreadTaskRunner* callback_runner,
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& database_path,
    const CreateCallback& callback) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(task_runner));
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner(database_path);
  if (status != SYNC_STATUS_OK)
    metadata_database.reset();

  callback_runner->PostTask(FROM_HERE, base::Bind(
      callback, status, base::Passed(&metadata_database)));
}

// static
SyncStatusCode MetadataDatabase::CreateForTesting(
    scoped_ptr<leveldb::DB> db,
    scoped_ptr<MetadataDatabase>* metadata_database_out) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(base::MessageLoopProxy::current()));
  metadata_database->db_ = db.Pass();
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner(base::FilePath());
  if (status == SYNC_STATUS_OK)
    *metadata_database_out = metadata_database.Pass();
  return status;
}

SyncStatusCode MetadataDatabase::InitializeOnTaskRunner(
    const base::FilePath& database_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  bool created = false;
  // Open database unless |db_| is overridden for testing.
  if (!db_) {
    status = OpenDatabase(database_path, &db_, &created);
    if (status != SYNC_STATUS_OK)
      return status;
  }

  if (created) {
    status = WriteVersionInfo(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  } else {
    status = MigrateDatabaseIfNeeded(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  }

  DatabaseContents contents;
  status = ReadDatabaseContents(db_.get(), &contents);
  if (status != SYNC_STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  status = InitializeServiceMetadata(&contents, &batch);
  if (status != SYNC_STATUS_OK)
    return status;

  status = RemoveUnreachableFiles(&contents, &batch);
  if (status != SYNC_STATUS_OK)
    return status;

  status = LevelDBStatusToSyncStatusCode(
      db_->Write(leveldb::WriteOptions(), &batch));
  if (status != SYNC_STATUS_OK)
    return status;

  BuildIndexes(&contents);
  return status;
}

void MetadataDatabase::BuildIndexes(DatabaseContents* contents) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                                       const SyncStatusCallback& callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&leveldb::DB::Write,
                 base::Unretained(db_.get()),
                 leveldb::WriteOptions(),
                 base::Owned(batch.release())),
      base::Bind(&AdaptLevelDBStatusToSyncStatusCode, callback));
}

}  // namespace drive_backend
}  // namespace sync_file_system
