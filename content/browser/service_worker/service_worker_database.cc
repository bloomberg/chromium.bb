// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

// LevelDB database schema
// =======================
//
// Version 1 (in sorted order)
//   key: "DB_VERSION"
//   value: <int64 serialized as a string>
//
//   key: "NEXT_REGISTRATION_ID"
//   value: <int64 serialized as a string>
//
//   key: "NEXT_RESOURCE_ID"
//   value: <int64 serialized as a string>
//
//   key: "NEXT_VERSION_ID"
//   value: <int64 serialized as a string>

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

  if (!ReadInt64(kNextRegIdKey, &reg_id) ||
      !ReadInt64(kNextVerIdKey, &ver_id) ||
      !ReadInt64(kNextResIdKey, &res_id))
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
    DLOG(ERROR) << "Failed to open LevelDB database: " << status.ToString();
    is_disabled_ = true;
    return false;
  }
  db_.reset(db);

  if (IsEmpty() && !PopulateInitialData()) {
    DLOG(ERROR) << "Failed to populate the database.";
    is_disabled_ = true;
    db_.reset();
    return false;
  }
  return true;
}

bool ServiceWorkerDatabase::PopulateInitialData() {
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  batch->Put(kDatabaseVersionKey, base::Int64ToString(kCurrentSchemaVersion));
  batch->Put(kNextRegIdKey, "0");
  batch->Put(kNextResIdKey, "0");
  batch->Put(kNextVerIdKey, "0");
  return WriteBatch(batch.Pass());
}

bool ServiceWorkerDatabase::ReadInt64(
    const leveldb::Slice& key,
    int64* value_out) {
  DCHECK(value_out);

  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
  if (!status.ok()) {
    DLOG(ERROR) << "Failed to read data keyed by "
                << key.ToString() << ": " << status.ToString();
    is_disabled_ = true;
    if (status.IsCorruption())
      was_corruption_detected_ = true;
    return false;
  }

  int64 parsed = -1;
  if (!base::StringToInt64(value, &parsed)) {
    DLOG(ERROR) << "Database might be corrupted: "
                << key.ToString() << ", " << value;
    is_disabled_ = true;
    was_corruption_detected_ = true;
    return false;
  }

  *value_out = parsed;
  return true;
}

bool ServiceWorkerDatabase::WriteBatch(scoped_ptr<leveldb::WriteBatch> batch) {
  if (!batch)
    return true;

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch.get());
  if (status.ok())
    return true;

  DLOG(ERROR) << "Failed to write the batch: " << status.ToString();
  is_disabled_ = true;
  if (status.IsCorruption())
    was_corruption_detected_ = true;
  return false;
}

bool ServiceWorkerDatabase::IsOpen() {
  return db_.get() != NULL;
}

bool ServiceWorkerDatabase::IsEmpty() {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

}  // namespace content
