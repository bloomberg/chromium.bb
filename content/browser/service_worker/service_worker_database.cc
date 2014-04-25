// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include <string>

#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

// LevelDB database schema
// =======================
//
// NOTE
// - int64 value is serialized as a string by base::Int64ToString().
//
// Version 1 (in sorted order)
//   key: "DB_VERSION"
//   value: "1"
//
//   key: "NEXT_REGISTRATION_ID"
//   value: <int64 'next_available_registration_id'>
//
//   key: "NEXT_RESOURCE_ID"
//   value: <int64 'next_available_resource_id'>
//
//   key: "NEXT_VERSION_ID"
//   value: <int64 'next_available_version_id'>

namespace content {

namespace {

const char kDatabaseVersionKey[] = "DB_VERSION";
const char kNextRegIdKey[] = "NEXT_REGISTRATION_ID";
const char kNextResIdKey[] = "NEXT_RESOURCE_ID";
const char kNextVerIdKey[] = "NEXT_VERSION_ID";

const int64 kCurrentSchemaVersion = 1;

}  // namespace

ServiceWorkerDatabase::RegistrationData::RegistrationData()
    : registration_id(-1),
      version_id(-1),
      is_active(false),
      has_fetch_handler(false) {
}

ServiceWorkerDatabase::RegistrationData::~RegistrationData() {
}

ServiceWorkerDatabase::ServiceWorkerDatabase(const base::FilePath& path)
    : path_(path),
      is_disabled_(false),
      was_corruption_detected_(false) {
}

ServiceWorkerDatabase::~ServiceWorkerDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  db_.reset();
}

bool ServiceWorkerDatabase::GetNextAvailableIds(
    int64* next_avail_registration_id,
    int64* next_avail_version_id,
    int64* next_avail_resource_id) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(next_avail_registration_id);
  DCHECK(next_avail_version_id);
  DCHECK(next_avail_resource_id);

  if (!LazyOpen(false) || is_disabled_)
    return false;

  int64 reg_id = -1;
  int64 ver_id = -1;
  int64 res_id = -1;

  if (!ReadNextAvailableId(kNextRegIdKey, &reg_id) ||
      !ReadNextAvailableId(kNextVerIdKey, &ver_id) ||
      !ReadNextAvailableId(kNextResIdKey, &res_id))
    return false;

  *next_avail_registration_id = reg_id;
  *next_avail_version_id = ver_id;
  *next_avail_resource_id = res_id;
  return true;
}

bool ServiceWorkerDatabase::LazyOpen(bool create_if_needed) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (IsOpen())
    return true;

  // Do not try to open a database if we tried and failed once.
  if (is_disabled_)
    return false;

  // When |path_| is empty, open a database in-memory.
  bool use_in_memory_db = path_.empty();

  if (!create_if_needed) {
    // Avoid opening a database if it does not exist at the |path_|.
    if (use_in_memory_db ||
        !base::PathExists(path_) ||
        base::IsDirectoryEmpty(path_)) {
      return false;
    }
  }

  leveldb::Options options;
  options.create_if_missing = create_if_needed;
  if (use_in_memory_db) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  }

  leveldb::DB* db = NULL;
  leveldb::Status status =
      leveldb::DB::Open(options, path_.AsUTF8Unsafe(), &db);
  if (!status.ok()) {
    DCHECK(!db);
    // TODO(nhiroki): Should we retry to open the database?
    HandleError(FROM_HERE, status);
    return false;
  }
  db_.reset(db);

  if (IsEmpty() && !PopulateInitialData()) {
    DLOG(ERROR) << "Failed to populate the database.";
    db_.reset();
    return false;
  }
  return true;
}

bool ServiceWorkerDatabase::PopulateInitialData() {
  leveldb::WriteBatch batch;
  batch.Put(kDatabaseVersionKey, base::Int64ToString(kCurrentSchemaVersion));
  batch.Put(kNextRegIdKey, base::Int64ToString(0));
  batch.Put(kNextResIdKey, base::Int64ToString(0));
  batch.Put(kNextVerIdKey, base::Int64ToString(0));
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::ReadNextAvailableId(
    const char* id_key, int64* next_avail_id) {
  DCHECK(id_key);
  DCHECK(next_avail_id);

  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), id_key, &value);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }

  int64 parsed;
  if (!base::StringToInt64(value, &parsed)) {
    HandleError(FROM_HERE, leveldb::Status::Corruption("failed to parse"));
    return false;
  }

  *next_avail_id = parsed;
  return true;
}

bool ServiceWorkerDatabase::WriteBatch(leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK(!is_disabled_);
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool ServiceWorkerDatabase::IsOpen() {
  return db_.get() != NULL;
}

bool ServiceWorkerDatabase::IsEmpty() {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  // TODO(nhiroki): Handle an error.
  DCHECK(itr->status().ok());
  return !itr->Valid();
}

void ServiceWorkerDatabase::HandleError(
    const tracked_objects::Location& from_here,
    const leveldb::Status& status) {
  // TODO(nhiroki): Add an UMA histogram.
  DLOG(ERROR) << "Failed at: " << from_here.ToString()
              << " with error: " << status.ToString();
  is_disabled_ = true;
  if (status.IsCorruption())
    was_corruption_detected_ = true;
  db_.reset();
}

}  // namespace content
