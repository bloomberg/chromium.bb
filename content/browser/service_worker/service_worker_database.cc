// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include <string>

#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/service_worker/service_worker_database.pb.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

// LevelDB database schema
// =======================
//
// NOTE
// - int64 value is serialized as a string by base::Int64ToString().
// - GURL value is serialized as a string by GURL::spec().
//
// Version 1 (in sorted order)
//   key: "INITDATA_DB_VERSION"
//   value: "1"
//
//   key: "INITDATA_NEXT_REGISTRATION_ID"
//   value: <int64 'next_available_registration_id'>
//
//   key: "INITDATA_NEXT_RESOURCE_ID"
//   value: <int64 'next_available_resource_id'>
//
//   key: "INITDATA_NEXT_VERSION_ID"
//   value: <int64 'next_available_version_id'>
//
//   key: "INITDATA_UNIQUE_ORIGIN:" + <GURL 'origin'>
//   value: <empty>
//
//   key: "PRES:" + <int64 'purgeable_resource_id'>
//   value: <empty>
//
//   key: "REG:" + <GURL 'origin'> + '\x00' + <int64 'registration_id'>
//     (ex. "REG:http://example.com\x00123456")
//   value: <ServiceWorkerRegistrationData serialized as a string>
//
//   key: "URES:" + <int64 'uncommitted_resource_id'>
//   value: <empty>

namespace content {

namespace {

const char kDatabaseVersionKey[] = "INITDATA_DB_VERSION";
const char kNextRegIdKey[] = "INITDATA_NEXT_REGISTRATION_ID";
const char kNextResIdKey[] = "INITDATA_NEXT_RESOURCE_ID";
const char kNextVerIdKey[] = "INITDATA_NEXT_VERSION_ID";
const char kUniqueOriginKey[] = "INITDATA_UNIQUE_ORIGIN:";

const char kRegKeyPrefix[] = "REG:";
const char kKeySeparator = '\x00';

const char kUncommittedResIdKeyPrefix[] = "URES:";
const char kPurgeableResIdKeyPrefix[] = "PRES:";

const int64 kCurrentSchemaVersion = 1;

bool RemovePrefix(const std::string& str,
                  const std::string& prefix,
                  std::string* out) {
  if (!StartsWithASCII(str, prefix, true))
    return false;
  if (out)
    *out = str.substr(prefix.size());
  return true;
}

std::string CreateRegistrationKey(int64 registration_id,
                                  const GURL& origin) {
  return base::StringPrintf("%s%s%c%s",
                            kRegKeyPrefix,
                            origin.spec().c_str(),
                            kKeySeparator,
                            base::Int64ToString(registration_id).c_str());
}

std::string CreateUniqueOriginKey(const GURL& origin) {
  return base::StringPrintf("%s%s", kUniqueOriginKey, origin.spec().c_str());
}

std::string CreateResourceIdKey(const char* key_prefix, int64 resource_id) {
  return base::StringPrintf(
      "%s%s", key_prefix, base::Int64ToString(resource_id).c_str());
}

void PutRegistrationDataToBatch(
    const ServiceWorkerDatabase::RegistrationData& input,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);

  // Convert RegistrationData to ServiceWorkerRegistrationData.
  ServiceWorkerRegistrationData data;
  data.set_registration_id(input.registration_id);
  data.set_scope_url(input.scope.spec());
  data.set_script_url(input.script.spec());
  data.set_version_id(input.version_id);
  data.set_is_active(input.is_active);
  data.set_has_fetch_handler(input.has_fetch_handler);
  data.set_last_update_check_time(input.last_update_check.ToInternalValue());

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  GURL origin = input.scope.GetOrigin();
  batch->Put(CreateRegistrationKey(data.registration_id(), origin), value);
}

void PutUniqueOriginToBatch(const GURL& origin,
                            leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateUniqueOriginKey(origin), "");
}

bool ParseRegistrationData(
    const std::string& serialized,
    ServiceWorkerDatabase::RegistrationData* out) {
  DCHECK(out);
  ServiceWorkerRegistrationData data;
  if (!data.ParseFromString(serialized))
    return false;

  GURL scope_url(data.scope_url());
  GURL script_url(data.script_url());
  if (scope_url.GetOrigin() != script_url.GetOrigin())
    return false;

  // Convert ServiceWorkerRegistrationData to RegistrationData.
  out->registration_id = data.registration_id();
  out->scope = scope_url;
  out->script = script_url;
  out->version_id = data.version_id();
  out->is_active = data.is_active();
  out->has_fetch_handler = data.has_fetch_handler();
  out->last_update_check =
      base::Time::FromInternalValue(data.last_update_check_time());
  return true;
}

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
      next_avail_registration_id_(0),
      next_avail_resource_id_(0),
      next_avail_version_id_(0),
      is_disabled_(false),
      was_corruption_detected_(false),
      is_initialized_(false) {
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

  if (!ReadNextAvailableId(kNextRegIdKey, &next_avail_registration_id_) ||
      !ReadNextAvailableId(kNextVerIdKey, &next_avail_version_id_) ||
      !ReadNextAvailableId(kNextResIdKey, &next_avail_resource_id_)) {
    return false;
  }

  *next_avail_registration_id = next_avail_registration_id_;
  *next_avail_version_id = next_avail_version_id_;
  *next_avail_resource_id = next_avail_resource_id_;
  return true;
}

bool ServiceWorkerDatabase::GetOriginsWithRegistrations(
    std::set<GURL>* origins) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(origins);

  if (!LazyOpen(false) || is_disabled_)
    return false;

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kUniqueOriginKey); itr->Valid(); itr->Next()) {
    if (!itr->status().ok()) {
      HandleError(FROM_HERE, itr->status());
      origins->clear();
      return false;
    }

    std::string origin;
    if (!RemovePrefix(itr->key().ToString(), kUniqueOriginKey, &origin))
      break;
    origins->insert(GURL(origin));
  }
  return true;
}

bool ServiceWorkerDatabase::GetRegistrationsForOrigin(
    const GURL& origin,
    std::vector<RegistrationData>* registrations) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registrations);

  if (!LazyOpen(false) || is_disabled_)
    return false;

  // Create a key prefix for registrations.
  std::string prefix = base::StringPrintf(
      "%s%s%c", kRegKeyPrefix, origin.spec().c_str(), kKeySeparator);

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
    if (!itr->status().ok()) {
      HandleError(FROM_HERE, itr->status());
      registrations->clear();
      return false;
    }

    if (!RemovePrefix(itr->key().ToString(), prefix, NULL))
      break;

    RegistrationData registration;
    if (!ParseRegistrationData(itr->value().ToString(), &registration)) {
      HandleError(FROM_HERE, leveldb::Status::Corruption("failed to parse"));
      registrations->clear();
      return false;
    }
    registrations->push_back(registration);
  }
  return true;
}

bool ServiceWorkerDatabase::ReadRegistration(
    int64 registration_id,
    const GURL& origin,
    RegistrationData* registration,
    std::vector<ResourceRecord>* resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registration);
  DCHECK(resources);

  if (!LazyOpen(false) || is_disabled_)
    return false;

  RegistrationData value;
  if (!ReadRegistrationData(registration_id, origin, &value))
    return false;

  // TODO(nhiroki): Read ResourceRecords tied with this registration.

  *registration = value;
  return true;
}

bool ServiceWorkerDatabase::WriteRegistration(
    const RegistrationData& registration,
    const std::vector<ResourceRecord>& resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!LazyOpen(true) || is_disabled_)
    return false;

  leveldb::WriteBatch batch;
  BumpNextRegistrationIdIfNeeded(registration.registration_id, &batch);
  BumpNextVersionIdIfNeeded(registration.version_id, &batch);

  // TODO(nhiroki): Skip to add the origin into the unique origin list if it
  // has already been added.
  PutUniqueOriginToBatch(registration.scope.GetOrigin(), &batch);

  PutRegistrationDataToBatch(registration, &batch);

  // TODO(nhiroki): Write |resources| into the database.
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::UpdateVersionToActive(int64 registration_id,
                                                  const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!LazyOpen(false) || is_disabled_)
    return false;

  RegistrationData registration;
  if (!ReadRegistrationData(registration_id, origin, &registration))
    return false;

  registration.is_active = true;

  leveldb::WriteBatch batch;
  PutRegistrationDataToBatch(registration, &batch);
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::UpdateLastCheckTime(int64 registration_id,
                                                const GURL& origin,
                                                const base::Time& time) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!LazyOpen(false) || is_disabled_)
    return false;

  RegistrationData registration;
  if (!ReadRegistrationData(registration_id, origin, &registration))
    return false;

  registration.last_update_check = time;

  leveldb::WriteBatch batch;
  PutRegistrationDataToBatch(registration, &batch);
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::DeleteRegistration(int64 registration_id,
                                               const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!LazyOpen(false) || is_disabled_)
    return false;

  leveldb::WriteBatch batch;

  // Remove |origin| from unique origins if a registration specified by
  // |registration_id| is the only one for |origin|.
  // TODO(nhiroki): Check the uniqueness by more efficient way.
  std::vector<RegistrationData> registrations;
  if (!GetRegistrationsForOrigin(origin, &registrations))
    return false;
  if (registrations.size() == 1 &&
      registrations[0].registration_id == registration_id) {
    batch.Delete(CreateUniqueOriginKey(origin));
  }

  batch.Delete(CreateRegistrationKey(registration_id, origin));

  // TODO(nhiroki): Delete ResourceRecords tied with this registration.
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::GetUncommittedResourceIds(std::set<int64>* ids) {
  return ReadResourceIds(kUncommittedResIdKeyPrefix, ids);
}

bool ServiceWorkerDatabase::WriteUncommittedResourceIds(
    const std::set<int64>& ids) {
  return WriteResourceIds(kUncommittedResIdKeyPrefix, ids);
}

bool ServiceWorkerDatabase::ClearUncommittedResourceIds(
    const std::set<int64>& ids) {
  return DeleteResourceIds(kUncommittedResIdKeyPrefix, ids);
}

bool ServiceWorkerDatabase::GetPurgeableResourceIds(std::set<int64>* ids) {
  return ReadResourceIds(kPurgeableResIdKeyPrefix, ids);
}

bool ServiceWorkerDatabase::WritePurgeableResourceIds(
    const std::set<int64>& ids) {
  return WriteResourceIds(kPurgeableResIdKeyPrefix, ids);
}

bool ServiceWorkerDatabase::ClearPurgeableResourceIds(
    const std::set<int64>& ids) {
  return DeleteResourceIds(kPurgeableResIdKeyPrefix, ids);
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

  int64 db_version;
  if (!ReadDatabaseVersion(&db_version))
    return false;
  if (db_version > 0)
    is_initialized_ = true;
  return true;
}

bool ServiceWorkerDatabase::ReadNextAvailableId(
    const char* id_key, int64* next_avail_id) {
  DCHECK(id_key);
  DCHECK(next_avail_id);

  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), id_key, &value);
  if (status.IsNotFound()) {
    // Nobody has gotten the next resource id for |id_key|.
    *next_avail_id = 0;
    return true;
  }

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

bool ServiceWorkerDatabase::ReadRegistrationData(
    int64 registration_id,
    const GURL& origin,
    RegistrationData* registration) {
  DCHECK(registration);

  std::string key = CreateRegistrationKey(registration_id, origin);

  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
  if (!status.ok()) {
    if (!status.IsNotFound())
      HandleError(FROM_HERE, status);
    return false;
  }

  RegistrationData parsed;
  if (!ParseRegistrationData(value, &parsed)) {
    HandleError(FROM_HERE, leveldb::Status::Corruption("failed to parse"));
    return false;
  }

  *registration = parsed;
  return true;
}

bool ServiceWorkerDatabase::ReadResourceIds(const char* id_key_prefix,
                                            std::set<int64>* ids) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);
  DCHECK(ids);

  if (!LazyOpen(false) || is_disabled_)
    return false;

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(id_key_prefix); itr->Valid(); itr->Next()) {
    if (!itr->status().ok()) {
      HandleError(FROM_HERE, itr->status());
      ids->clear();
      return false;
    }

    std::string unprefixed;
    if (!RemovePrefix(itr->key().ToString(), id_key_prefix, &unprefixed))
      break;

    int64 resource_id;
    if (!base::StringToInt64(unprefixed, &resource_id)) {
      HandleError(FROM_HERE, leveldb::Status::Corruption("failed to parse"));
      ids->clear();
      return false;
    }
    ids->insert(resource_id);
  }
  return true;
}

bool ServiceWorkerDatabase::WriteResourceIds(const char* id_key_prefix,
                                             const std::set<int64>& ids) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);

  if (!LazyOpen(true) || is_disabled_)
    return false;
  if (ids.empty())
    return true;

  leveldb::WriteBatch batch;
  for (std::set<int64>::const_iterator itr = ids.begin();
       itr != ids.end(); ++itr) {
    // Value should be empty.
    batch.Put(CreateResourceIdKey(id_key_prefix, *itr), "");
  }
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::DeleteResourceIds(const char* id_key_prefix,
                                              const std::set<int64>& ids) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);

  if (!LazyOpen(true) || is_disabled_)
    return false;
  if (ids.empty())
    return true;

  leveldb::WriteBatch batch;
  for (std::set<int64>::const_iterator itr = ids.begin();
       itr != ids.end(); ++itr) {
    batch.Delete(CreateResourceIdKey(id_key_prefix, *itr));
  }
  return WriteBatch(&batch);
}

bool ServiceWorkerDatabase::ReadDatabaseVersion(int64* db_version) {
  std::string value;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), kDatabaseVersionKey, &value);
  if (status.IsNotFound()) {
    // The database hasn't been initialized yet.
    *db_version = 0;
    return true;
  }
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }

  int64 parsed;
  if (!base::StringToInt64(value, &parsed)) {
    HandleError(FROM_HERE, leveldb::Status::Corruption("failed to parse"));
    return false;
  }

  const int kFirstValidVersion = 1;
  if (parsed < kFirstValidVersion || kCurrentSchemaVersion < parsed) {
    HandleError(FROM_HERE, leveldb::Status::Corruption("invalid DB version"));
    return false;
  }

  *db_version = parsed;
  return true;
}

bool ServiceWorkerDatabase::WriteBatch(leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK(!is_disabled_);

  if (!is_initialized_) {
    // Write the database schema version.
    batch->Put(kDatabaseVersionKey, base::Int64ToString(kCurrentSchemaVersion));
    is_initialized_ = true;
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

void ServiceWorkerDatabase::BumpNextRegistrationIdIfNeeded(
    int64 used_id, leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_registration_id_ <= used_id) {
    next_avail_registration_id_ = used_id + 1;
    batch->Put(kNextRegIdKey, base::Int64ToString(next_avail_registration_id_));
  }
}

void ServiceWorkerDatabase::BumpNextVersionIdIfNeeded(
    int64 used_id, leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_version_id_ <= used_id) {
    next_avail_version_id_ = used_id + 1;
    batch->Put(kNextVerIdKey, base::Int64ToString(next_avail_version_id_));
  }
}

bool ServiceWorkerDatabase::IsOpen() {
  return db_.get() != NULL;
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
