// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/service_worker/service_worker_database.pb.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/service_worker/service_worker_types.h"
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
//   key: "REG_HAS_USER_DATA:" + <std::string 'user_data_name'> + '\x00'
//            + <int64 'registration_id'>
//   value: <empty>
//
//   key: "REG_USER_DATA:" + <int64 'registration_id'> + '\x00'
//            + <std::string user_data_name>
//     (ex. "REG_USER_DATA:123456\x00foo_bar")
//   value: <std::string user_data>
//
//   key: "RES:" + <int64 'version_id'> + '\x00' + <int64 'resource_id'>
//     (ex. "RES:123456\x00654321")
//   value: <ServiceWorkerResourceRecord serialized as a string>
//
//   key: "URES:" + <int64 'uncommitted_resource_id'>
//   value: <empty>
//
// Version 2
//
//   key: "REGID_TO_ORIGIN:" + <int64 'registration_id'>
//   value: <GURL 'origin'>
namespace content {

namespace {

const char kDatabaseVersionKey[] = "INITDATA_DB_VERSION";
const char kNextRegIdKey[] = "INITDATA_NEXT_REGISTRATION_ID";
const char kNextResIdKey[] = "INITDATA_NEXT_RESOURCE_ID";
const char kNextVerIdKey[] = "INITDATA_NEXT_VERSION_ID";
const char kUniqueOriginKey[] = "INITDATA_UNIQUE_ORIGIN:";

const char kRegKeyPrefix[] = "REG:";
const char kRegUserDataKeyPrefix[] = "REG_USER_DATA:";
const char kRegHasUserDataKeyPrefix[] = "REG_HAS_USER_DATA:";
const char kRegIdToOriginKeyPrefix[] = "REGID_TO_ORIGIN:";
const char kResKeyPrefix[] = "RES:";
const char kKeySeparator = '\x00';

const char kUncommittedResIdKeyPrefix[] = "URES:";
const char kPurgeableResIdKeyPrefix[] = "PRES:";

const int64 kCurrentSchemaVersion = 2;

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

std::string CreateResourceRecordKeyPrefix(int64 version_id) {
  return base::StringPrintf("%s%s%c",
                            kResKeyPrefix,
                            base::Int64ToString(version_id).c_str(),
                            kKeySeparator);
}

std::string CreateResourceRecordKey(int64 version_id,
                                    int64 resource_id) {
  return CreateResourceRecordKeyPrefix(version_id).append(
      base::Int64ToString(resource_id));
}

std::string CreateUniqueOriginKey(const GURL& origin) {
  return base::StringPrintf("%s%s", kUniqueOriginKey, origin.spec().c_str());
}

std::string CreateResourceIdKey(const char* key_prefix, int64 resource_id) {
  return base::StringPrintf(
      "%s%s", key_prefix, base::Int64ToString(resource_id).c_str());
}

std::string CreateUserDataKeyPrefix(int64 registration_id) {
  return base::StringPrintf("%s%s%c",
                            kRegUserDataKeyPrefix,
                            base::Int64ToString(registration_id).c_str(),
                            kKeySeparator);
}

std::string CreateUserDataKey(int64 registration_id,
                              const std::string& user_data_name) {
  return CreateUserDataKeyPrefix(registration_id).append(user_data_name);
}

std::string CreateHasUserDataKeyPrefix(const std::string& user_data_name) {
  return base::StringPrintf("%s%s%c", kRegHasUserDataKeyPrefix,
                            user_data_name.c_str(), kKeySeparator);
}

std::string CreateHasUserDataKey(int64 registration_id,
                                 const std::string& user_data_name) {
  return CreateHasUserDataKeyPrefix(user_data_name)
      .append(base::Int64ToString(registration_id));
}

std::string CreateRegistrationIdToOriginKey(int64 registration_id) {
  return base::StringPrintf("%s%s", kRegIdToOriginKeyPrefix,
                            base::Int64ToString(registration_id).c_str());
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
  data.set_resources_total_size_bytes(input.resources_total_size_bytes);

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  GURL origin = input.scope.GetOrigin();
  batch->Put(CreateRegistrationKey(data.registration_id(), origin), value);
}

void PutResourceRecordToBatch(
    const ServiceWorkerDatabase::ResourceRecord& input,
    int64 version_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK_GE(input.size_bytes, 0);

  // Convert ResourceRecord to ServiceWorkerResourceRecord.
  ServiceWorkerResourceRecord record;
  record.set_resource_id(input.resource_id);
  record.set_url(input.url.spec());
  record.set_size_bytes(input.size_bytes);

  std::string value;
  bool success = record.SerializeToString(&value);
  DCHECK(success);
  batch->Put(CreateResourceRecordKey(version_id, input.resource_id), value);
}

void PutUniqueOriginToBatch(const GURL& origin,
                            leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateUniqueOriginKey(origin), "");
}

void PutPurgeableResourceIdToBatch(int64 resource_id,
                                   leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateResourceIdKey(kPurgeableResIdKeyPrefix, resource_id), "");
}

ServiceWorkerDatabase::Status ParseId(
    const std::string& serialized,
    int64* out) {
  DCHECK(out);
  int64 id;
  if (!base::StringToInt64(serialized, &id) || id < 0)
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  *out = id;
  return ServiceWorkerDatabase::STATUS_OK;
}

ServiceWorkerDatabase::Status ParseDatabaseVersion(
    const std::string& serialized,
    int64* out) {
  DCHECK(out);
  const int kFirstValidVersion = 1;
  int64 version;
  if (!base::StringToInt64(serialized, &version) ||
      version < kFirstValidVersion) {
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  }
  if (kCurrentSchemaVersion < version) {
    DLOG(ERROR) << "ServiceWorkerDatabase has newer schema version"
                << " than the current latest version: "
                << version << " vs " << kCurrentSchemaVersion;
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  }
  *out = version;
  return ServiceWorkerDatabase::STATUS_OK;
}

ServiceWorkerDatabase::Status ParseRegistrationData(
    const std::string& serialized,
    ServiceWorkerDatabase::RegistrationData* out) {
  DCHECK(out);
  ServiceWorkerRegistrationData data;
  if (!data.ParseFromString(serialized))
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  GURL scope_url(data.scope_url());
  GURL script_url(data.script_url());
  if (!scope_url.is_valid() ||
      !script_url.is_valid() ||
      scope_url.GetOrigin() != script_url.GetOrigin()) {
    DLOG(ERROR) << "Scope URL '" << data.scope_url() << "' and/or script url '"
                << data.script_url()
                << "' are invalid or have mismatching origins.";
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  }

  // Convert ServiceWorkerRegistrationData to RegistrationData.
  out->registration_id = data.registration_id();
  out->scope = scope_url;
  out->script = script_url;
  out->version_id = data.version_id();
  out->is_active = data.is_active();
  out->has_fetch_handler = data.has_fetch_handler();
  out->last_update_check =
      base::Time::FromInternalValue(data.last_update_check_time());
  out->resources_total_size_bytes = data.resources_total_size_bytes();

  return ServiceWorkerDatabase::STATUS_OK;
}

ServiceWorkerDatabase::Status ParseResourceRecord(
    const std::string& serialized,
    ServiceWorkerDatabase::ResourceRecord* out) {
  DCHECK(out);
  ServiceWorkerResourceRecord record;
  if (!record.ParseFromString(serialized))
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  GURL url(record.url());
  if (!url.is_valid())
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  // Convert ServiceWorkerResourceRecord to ResourceRecord.
  out->resource_id = record.resource_id();
  out->url = url;
  out->size_bytes = record.size_bytes();
  return ServiceWorkerDatabase::STATUS_OK;
}

ServiceWorkerDatabase::Status LevelDBStatusToStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return ServiceWorkerDatabase::STATUS_OK;
  else if (status.IsNotFound())
    return ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND;
  else if (status.IsIOError())
    return ServiceWorkerDatabase::STATUS_ERROR_IO_ERROR;
  else if (status.IsCorruption())
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  else
    return ServiceWorkerDatabase::STATUS_ERROR_FAILED;
}

}  // namespace

const char* ServiceWorkerDatabase::StatusToString(
  ServiceWorkerDatabase::Status status) {
  switch (status) {
    case ServiceWorkerDatabase::STATUS_OK:
      return "Database OK";
    case ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND:
      return "Database not found";
    case ServiceWorkerDatabase::STATUS_ERROR_IO_ERROR:
      return "Database IO error";
    case ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED:
      return "Database corrupted";
    case ServiceWorkerDatabase::STATUS_ERROR_FAILED:
      return "Database operation failed";
    case ServiceWorkerDatabase::STATUS_ERROR_MAX:
      NOTREACHED();
      return "Database unknown error";
  }
  NOTREACHED();
  return "Database unknown error";
}

ServiceWorkerDatabase::RegistrationData::RegistrationData()
    : registration_id(kInvalidServiceWorkerRegistrationId),
      version_id(kInvalidServiceWorkerVersionId),
      is_active(false),
      has_fetch_handler(false),
      resources_total_size_bytes(0) {
}

ServiceWorkerDatabase::RegistrationData::~RegistrationData() {
}

ServiceWorkerDatabase::ServiceWorkerDatabase(const base::FilePath& path)
    : path_(path),
      next_avail_registration_id_(0),
      next_avail_resource_id_(0),
      next_avail_version_id_(0),
      state_(UNINITIALIZED) {
  sequence_checker_.DetachFromSequence();
}

ServiceWorkerDatabase::~ServiceWorkerDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  db_.reset();
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetNextAvailableIds(
    int64* next_avail_registration_id,
    int64* next_avail_version_id,
    int64* next_avail_resource_id) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(next_avail_registration_id);
  DCHECK(next_avail_version_id);
  DCHECK(next_avail_resource_id);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status)) {
    *next_avail_registration_id = 0;
    *next_avail_version_id = 0;
    *next_avail_resource_id = 0;
    return STATUS_OK;
  }
  if (status != STATUS_OK)
    return status;

  status = ReadNextAvailableId(kNextRegIdKey, &next_avail_registration_id_);
  if (status != STATUS_OK)
    return status;
  status = ReadNextAvailableId(kNextVerIdKey, &next_avail_version_id_);
  if (status != STATUS_OK)
    return status;
  status = ReadNextAvailableId(kNextResIdKey, &next_avail_resource_id_);
  if (status != STATUS_OK)
    return status;

  *next_avail_registration_id = next_avail_registration_id_;
  *next_avail_version_id = next_avail_version_id_;
  *next_avail_resource_id = next_avail_resource_id_;
  return STATUS_OK;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetOriginsWithRegistrations(std::set<GURL>* origins) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(origins->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kUniqueOriginKey); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      origins->clear();
      return status;
    }

    std::string origin;
    if (!RemovePrefix(itr->key().ToString(), kUniqueOriginKey, &origin))
      break;
    origins->insert(GURL(origin));
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetRegistrationsForOrigin(
    const GURL& origin,
    std::vector<RegistrationData>* registrations) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registrations->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  // Create a key prefix for registrations.
  std::string prefix = base::StringPrintf(
      "%s%s%c", kRegKeyPrefix, origin.spec().c_str(), kKeySeparator);

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      registrations->clear();
      return status;
    }

    if (!RemovePrefix(itr->key().ToString(), prefix, NULL))
      break;

    RegistrationData registration;
    status = ParseRegistrationData(itr->value().ToString(), &registration);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      registrations->clear();
      return status;
    }
    registrations->push_back(registration);
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetAllRegistrations(
    std::vector<RegistrationData>* registrations) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registrations->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kRegKeyPrefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      registrations->clear();
      return status;
    }

    if (!RemovePrefix(itr->key().ToString(), kRegKeyPrefix, NULL))
      break;

    RegistrationData registration;
    status = ParseRegistrationData(itr->value().ToString(), &registration);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      registrations->clear();
      return status;
    }
    registrations->push_back(registration);
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistration(
    int64 registration_id,
    const GURL& origin,
    RegistrationData* registration,
    std::vector<ResourceRecord>* resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registration);
  DCHECK(resources);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  RegistrationData value;
  status = ReadRegistrationData(registration_id, origin, &value);
  if (status != STATUS_OK)
    return status;

  status = ReadResourceRecords(value.version_id, resources);
  if (status != STATUS_OK)
    return status;

  *registration = value;
  return STATUS_OK;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistrationOrigin(
    int64 registration_id,
    GURL* origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(origin);

  Status status = LazyOpen(true);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  std::string value;
  status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(),
               CreateRegistrationIdToOriginKey(registration_id), &value));
  if (status != STATUS_OK) {
    HandleReadResult(FROM_HERE,
                     status == STATUS_ERROR_NOT_FOUND ? STATUS_OK : status);
    return status;
  }

  GURL parsed(value);
  if (!parsed.is_valid()) {
    status = STATUS_ERROR_CORRUPTED;
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  *origin = parsed;
  HandleReadResult(FROM_HERE, STATUS_OK);
  return STATUS_OK;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteRegistration(
    const RegistrationData& registration,
    const std::vector<ResourceRecord>& resources,
    RegistrationData* old_registration,
    std::vector<int64>* newly_purgeable_resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(old_registration);
  Status status = LazyOpen(true);
  old_registration->version_id = kInvalidServiceWorkerVersionId;
  if (status != STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  BumpNextRegistrationIdIfNeeded(registration.registration_id, &batch);
  BumpNextVersionIdIfNeeded(registration.version_id, &batch);

  PutUniqueOriginToBatch(registration.scope.GetOrigin(), &batch);
#if DCHECK_IS_ON()
  int64_t total_size_bytes = 0;
  for (const auto& resource : resources) {
    total_size_bytes += resource.size_bytes;
  }
  DCHECK_EQ(total_size_bytes, registration.resources_total_size_bytes)
      << "The total size in the registration must match the cumulative "
      << "sizes of the resources.";
#endif
  PutRegistrationDataToBatch(registration, &batch);
  batch.Put(CreateRegistrationIdToOriginKey(registration.registration_id),
            registration.scope.GetOrigin().spec());

  // Used for avoiding multiple writes for the same resource id or url.
  std::set<int64> pushed_resources;
  std::set<GURL> pushed_urls;
  for (std::vector<ResourceRecord>::const_iterator itr = resources.begin();
       itr != resources.end(); ++itr) {
    if (!itr->url.is_valid())
      return STATUS_ERROR_FAILED;

    // Duplicated resource id or url should not exist.
    DCHECK(pushed_resources.insert(itr->resource_id).second);
    DCHECK(pushed_urls.insert(itr->url).second);

    PutResourceRecordToBatch(*itr, registration.version_id, &batch);

    // Delete a resource from the uncommitted list.
    batch.Delete(CreateResourceIdKey(
        kUncommittedResIdKeyPrefix, itr->resource_id));
    // Delete from the purgeable list in case this version was once deleted.
    batch.Delete(
        CreateResourceIdKey(kPurgeableResIdKeyPrefix, itr->resource_id));
  }

  // Retrieve a previous version to sweep purgeable resources.
  status = ReadRegistrationData(registration.registration_id,
                                registration.scope.GetOrigin(),
                                old_registration);
  if (status != STATUS_OK && status != STATUS_ERROR_NOT_FOUND)
    return status;
  if (status == STATUS_OK) {
    DCHECK_LT(old_registration->version_id, registration.version_id);
    status = DeleteResourceRecords(
        old_registration->version_id, newly_purgeable_resources, &batch);
    if (status != STATUS_OK)
      return status;

    // Currently resource sharing across versions and registrations is not
    // supported, so resource ids should not be overlapped between
    // |registration| and |old_registration|.
    std::set<int64> deleted_resources(newly_purgeable_resources->begin(),
                                      newly_purgeable_resources->end());
    DCHECK(base::STLSetIntersection<std::set<int64> >(
        pushed_resources, deleted_resources).empty());
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateVersionToActive(
    int64 registration_id,
    const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;
  if (!origin.is_valid())
    return STATUS_ERROR_FAILED;

  RegistrationData registration;
  status = ReadRegistrationData(registration_id, origin, &registration);
  if (status != STATUS_OK)
    return status;

  registration.is_active = true;

  leveldb::WriteBatch batch;
  PutRegistrationDataToBatch(registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateLastCheckTime(
    int64 registration_id,
    const GURL& origin,
    const base::Time& time) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;
  if (!origin.is_valid())
    return STATUS_ERROR_FAILED;

  RegistrationData registration;
  status = ReadRegistrationData(registration_id, origin, &registration);
  if (status != STATUS_OK)
    return status;

  registration.last_update_check = time;

  leveldb::WriteBatch batch;
  PutRegistrationDataToBatch(registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteRegistration(
    int64 registration_id,
    const GURL& origin,
    RegistrationData* deleted_version,
    std::vector<int64>* newly_purgeable_resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(deleted_version);
  deleted_version->version_id = kInvalidServiceWorkerVersionId;
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;
  if (!origin.is_valid())
    return STATUS_ERROR_FAILED;

  leveldb::WriteBatch batch;

  // Remove |origin| from unique origins if a registration specified by
  // |registration_id| is the only one for |origin|.
  // TODO(nhiroki): Check the uniqueness by more efficient way.
  std::vector<RegistrationData> registrations;
  status = GetRegistrationsForOrigin(origin, &registrations);
  if (status != STATUS_OK)
    return status;

  if (registrations.size() == 1 &&
      registrations[0].registration_id == registration_id) {
    batch.Delete(CreateUniqueOriginKey(origin));
  }

  // Delete a registration specified by |registration_id|.
  batch.Delete(CreateRegistrationKey(registration_id, origin));
  batch.Delete(CreateRegistrationIdToOriginKey(registration_id));

  // Delete resource records and user data associated with the registration.
  for (const auto& registration : registrations) {
    if (registration.registration_id == registration_id) {
      *deleted_version = registration;
      status = DeleteResourceRecords(
          registration.version_id, newly_purgeable_resources, &batch);
      if (status != STATUS_OK)
        return status;

      status = DeleteUserDataForRegistration(registration_id, &batch);
      if (status != STATUS_OK)
        return status;
      break;
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadUserData(
    int64 registration_id,
    const std::string& user_data_name,
    std::string* user_data) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_name.empty());
  DCHECK(user_data);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  const std::string key = CreateUserDataKey(registration_id, user_data_name);
  status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), key, user_data));
  HandleReadResult(FROM_HERE,
                   status == STATUS_ERROR_NOT_FOUND ? STATUS_OK : status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteUserData(
    int64 registration_id,
    const GURL& origin,
    const std::string& user_data_name,
    const std::string& user_data) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_name.empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  // There should be the registration specified by |registration_id|.
  RegistrationData registration;
  status = ReadRegistrationData(registration_id, origin, &registration);
  if (status != STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  batch.Put(CreateUserDataKey(registration_id, user_data_name), user_data);
  batch.Put(CreateHasUserDataKey(registration_id, user_data_name), "");
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteUserData(
    int64 registration_id,
    const std::string& user_data_name) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_name.empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  batch.Delete(CreateUserDataKey(registration_id, user_data_name));
  batch.Delete(CreateHasUserDataKey(registration_id, user_data_name));
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ReadUserDataForAllRegistrations(
    const std::string& user_data_name,
    std::vector<std::pair<int64, std::string>>* user_data) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(user_data->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  std::string key_prefix = CreateHasUserDataKeyPrefix(user_data_name);
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(key_prefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      user_data->clear();
      return status;
    }

    std::string registration_id_string;
    if (!RemovePrefix(itr->key().ToString(), key_prefix,
                      &registration_id_string)) {
      break;
    }

    int64 registration_id;
    status = ParseId(registration_id_string, &registration_id);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      user_data->clear();
      return status;
    }

    std::string value;
    status = LevelDBStatusToStatus(
        db_->Get(leveldb::ReadOptions(),
                 CreateUserDataKey(registration_id, user_data_name), &value));
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      user_data->clear();
      return status;
    }
    user_data->push_back(std::make_pair(registration_id, value));
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetUncommittedResourceIds(std::set<int64>* ids) {
  return ReadResourceIds(kUncommittedResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::WriteUncommittedResourceIds(const std::set<int64>& ids) {
  return WriteResourceIds(kUncommittedResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ClearUncommittedResourceIds(const std::set<int64>& ids) {
  return DeleteResourceIds(kUncommittedResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetPurgeableResourceIds(std::set<int64>* ids) {
  return ReadResourceIds(kPurgeableResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::WritePurgeableResourceIds(const std::set<int64>& ids) {
  return WriteResourceIds(kPurgeableResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ClearPurgeableResourceIds(const std::set<int64>& ids) {
  return DeleteResourceIds(kPurgeableResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::PurgeUncommittedResourceIds(
    const std::set<int64>& ids) {
  leveldb::WriteBatch batch;
  Status status = DeleteResourceIdsInBatch(
      kUncommittedResIdKeyPrefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  status = WriteResourceIdsInBatch(kPurgeableResIdKeyPrefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteAllDataForOrigins(
    const std::set<GURL>& origins,
    std::vector<int64>* newly_purgeable_resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;
  leveldb::WriteBatch batch;

  for (const GURL& origin : origins) {
    if (!origin.is_valid())
      return STATUS_ERROR_FAILED;

    // Delete from the unique origin list.
    batch.Delete(CreateUniqueOriginKey(origin));

    std::vector<RegistrationData> registrations;
    status = GetRegistrationsForOrigin(origin, &registrations);
    if (status != STATUS_OK)
      return status;

    // Delete registrations, resource records and user data.
    for (const RegistrationData& data : registrations) {
      batch.Delete(CreateRegistrationKey(data.registration_id, origin));
      batch.Delete(CreateRegistrationIdToOriginKey(data.registration_id));

      status = DeleteResourceRecords(
          data.version_id, newly_purgeable_resources, &batch);
      if (status != STATUS_OK)
        return status;

      status = DeleteUserDataForRegistration(data.registration_id, &batch);
      if (status != STATUS_OK)
        return status;
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DestroyDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Disable(FROM_HERE, STATUS_OK);

  leveldb::Options options;
  if (path_.empty()) {
    if (env_) {
      options.env = env_.get();
    } else {
      // In-memory database not initialized.
      return STATUS_OK;
    }
  }

  return LevelDBStatusToStatus(
      leveldb::DestroyDB(path_.AsUTF8Unsafe(), options));
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::LazyOpen(
    bool create_if_missing) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  // Do not try to open a database if we tried and failed once.
  if (state_ == DISABLED)
    return STATUS_ERROR_FAILED;
  if (IsOpen())
    return STATUS_OK;

  // When |path_| is empty, open a database in-memory.
  bool use_in_memory_db = path_.empty();

  if (!create_if_missing) {
    // Avoid opening a database if it does not exist at the |path_|.
    if (use_in_memory_db ||
        !base::PathExists(path_) ||
        base::IsDirectoryEmpty(path_)) {
      return STATUS_ERROR_NOT_FOUND;
    }
  }

  leveldb::Options options;
  options.create_if_missing = create_if_missing;
  if (use_in_memory_db) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  }

  leveldb::DB* db = NULL;
  Status status = LevelDBStatusToStatus(
      leveldb::DB::Open(options, path_.AsUTF8Unsafe(), &db));
  HandleOpenResult(FROM_HERE, status);
  if (status != STATUS_OK) {
    DCHECK(!db);
    // TODO(nhiroki): Should we retry to open the database?
    return status;
  }
  db_.reset(db);

  int64 db_version;
  status = ReadDatabaseVersion(&db_version);
  if (status != STATUS_OK)
    return status;
  DCHECK_LE(0, db_version);

  if (db_version > 0 && db_version < kCurrentSchemaVersion) {
    switch (db_version) {
      case 1:
        status = UpgradeDatabaseSchemaFromV1ToV2();
        if (status != STATUS_OK)
          return status;
        db_version = 2;
        // Intentionally fall-through to other version upgrade cases.
    }
    // Either the database got upgraded to the current schema version, or some
    // upgrade step failed which would have caused this method to abort.
    DCHECK_EQ(db_version, kCurrentSchemaVersion);
  }

  if (db_version > 0)
    state_ = INITIALIZED;
  return STATUS_OK;
}

bool ServiceWorkerDatabase::IsNewOrNonexistentDatabase(
    ServiceWorkerDatabase::Status status) {
  if (status == STATUS_ERROR_NOT_FOUND)
    return true;
  if (status == STATUS_OK && state_ == UNINITIALIZED)
    return true;
  return false;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::UpgradeDatabaseSchemaFromV1ToV2() {
  Status status = STATUS_OK;
  leveldb::WriteBatch batch;

  // Version 2 introduced REGID_TO_ORIGIN, add for all existing registrations.
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kRegKeyPrefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      return status;
    }

    std::string key;
    if (!RemovePrefix(itr->key().ToString(), kRegKeyPrefix, &key))
      break;

    std::vector<std::string> parts;
    base::SplitStringDontTrim(key, kKeySeparator, &parts);
    if (parts.size() != 2) {
      status = STATUS_ERROR_CORRUPTED;
      HandleReadResult(FROM_HERE, status);
      return status;
    }

    int64 registration_id;
    status = ParseId(parts[1], &registration_id);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      return status;
    }

    batch.Put(CreateRegistrationIdToOriginKey(registration_id), parts[0]);
  }

  // Update schema version manually instead of relying on WriteBatch to make
  // sure each upgrade step only updates it to the actually correct version.
  batch.Put(kDatabaseVersionKey, base::Int64ToString(2));
  status = LevelDBStatusToStatus(
      db_->Write(leveldb::WriteOptions(), &batch));
  HandleWriteResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadNextAvailableId(
    const char* id_key,
    int64* next_avail_id) {
  DCHECK(id_key);
  DCHECK(next_avail_id);

  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), id_key, &value));
  if (status == STATUS_ERROR_NOT_FOUND) {
    // Nobody has gotten the next resource id for |id_key|.
    *next_avail_id = 0;
    HandleReadResult(FROM_HERE, STATUS_OK);
    return STATUS_OK;
  } else if (status != STATUS_OK) {
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  status = ParseId(value, next_avail_id);
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistrationData(
    int64 registration_id,
    const GURL& origin,
    RegistrationData* registration) {
  DCHECK(registration);

  const std::string key = CreateRegistrationKey(registration_id, origin);
  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), key, &value));
  if (status != STATUS_OK) {
    HandleReadResult(
        FROM_HERE,
        status == STATUS_ERROR_NOT_FOUND ? STATUS_OK : status);
    return status;
  }

  status = ParseRegistrationData(value, registration);
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadResourceRecords(
    int64 version_id,
    std::vector<ResourceRecord>* resources) {
  DCHECK(resources->empty());

  Status status = STATUS_OK;
  const std::string prefix = CreateResourceRecordKeyPrefix(version_id);

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
    Status status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      resources->clear();
      return status;
    }

    if (!RemovePrefix(itr->key().ToString(), prefix, NULL))
      break;

    ResourceRecord resource;
    status = ParseResourceRecord(itr->value().ToString(), &resource);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      resources->clear();
      return status;
    }
    resources->push_back(resource);
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceRecords(
    int64 version_id,
    std::vector<int64>* newly_purgeable_resources,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);

  Status status = STATUS_OK;
  const std::string prefix = CreateResourceRecordKeyPrefix(version_id);

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      return status;
    }

    const std::string key = itr->key().ToString();
    std::string unprefixed;
    if (!RemovePrefix(key, prefix, &unprefixed))
      break;

    int64 resource_id;
    status = ParseId(unprefixed, &resource_id);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      return status;
    }

    // Remove a resource record.
    batch->Delete(key);

    // Currently resource sharing across versions and registrations is not
    // supported, so we can purge this without caring about it.
    PutPurgeableResourceIdToBatch(resource_id, batch);
    newly_purgeable_resources->push_back(resource_id);
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadResourceIds(
    const char* id_key_prefix,
    std::set<int64>* ids) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);
  DCHECK(ids->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(id_key_prefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      ids->clear();
      return status;
    }

    std::string unprefixed;
    if (!RemovePrefix(itr->key().ToString(), id_key_prefix, &unprefixed))
      break;

    int64 resource_id;
    status = ParseId(unprefixed, &resource_id);
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      ids->clear();
      return status;
    }
    ids->insert(resource_id);
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteResourceIds(
    const char* id_key_prefix,
    const std::set<int64>& ids) {
  leveldb::WriteBatch batch;
  Status status = WriteResourceIdsInBatch(id_key_prefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteResourceIdsInBatch(
    const char* id_key_prefix,
    const std::set<int64>& ids,
    leveldb::WriteBatch* batch) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);

  Status status = LazyOpen(true);
  if (status != STATUS_OK)
    return status;

  if (ids.empty())
    return STATUS_OK;
  for (std::set<int64>::const_iterator itr = ids.begin();
       itr != ids.end(); ++itr) {
    // Value should be empty.
    batch->Put(CreateResourceIdKey(id_key_prefix, *itr), "");
  }
  // std::set is sorted, so the last element is the largest.
  BumpNextResourceIdIfNeeded(*ids.rbegin(), batch);
  return STATUS_OK;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceIds(
    const char* id_key_prefix,
    const std::set<int64>& ids) {
  leveldb::WriteBatch batch;
  Status status = DeleteResourceIdsInBatch(id_key_prefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceIdsInBatch(
    const char* id_key_prefix,
    const std::set<int64>& ids,
    leveldb::WriteBatch* batch) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  for (std::set<int64>::const_iterator itr = ids.begin();
       itr != ids.end(); ++itr) {
    batch->Delete(CreateResourceIdKey(id_key_prefix, *itr));
  }
  return STATUS_OK;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::DeleteUserDataForRegistration(
    int64 registration_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  Status status = STATUS_OK;
  const std::string prefix = CreateUserDataKeyPrefix(registration_id);

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToStatus(itr->status());
    if (status != STATUS_OK) {
      HandleReadResult(FROM_HERE, status);
      return status;
    }

    const std::string key = itr->key().ToString();
    std::string user_data_name;
    if (!RemovePrefix(key, prefix, &user_data_name))
      break;
    batch->Delete(key);
    batch->Delete(CreateHasUserDataKey(registration_id, user_data_name));
  }
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadDatabaseVersion(
    int64* db_version) {
  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), kDatabaseVersionKey, &value));
  if (status == STATUS_ERROR_NOT_FOUND) {
    // The database hasn't been initialized yet.
    *db_version = 0;
    HandleReadResult(FROM_HERE, STATUS_OK);
    return STATUS_OK;
  }

  if (status != STATUS_OK) {
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  status = ParseDatabaseVersion(value, db_version);
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteBatch(
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK_NE(DISABLED, state_);

  if (state_ == UNINITIALIZED) {
    // Write the database schema version.
    batch->Put(kDatabaseVersionKey, base::Int64ToString(kCurrentSchemaVersion));
    state_ = INITIALIZED;
  }

  Status status = LevelDBStatusToStatus(
      db_->Write(leveldb::WriteOptions(), batch));
  HandleWriteResult(FROM_HERE, status);
  return status;
}

void ServiceWorkerDatabase::BumpNextRegistrationIdIfNeeded(
    int64 used_id, leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_registration_id_ <= used_id) {
    next_avail_registration_id_ = used_id + 1;
    batch->Put(kNextRegIdKey, base::Int64ToString(next_avail_registration_id_));
  }
}

void ServiceWorkerDatabase::BumpNextResourceIdIfNeeded(
    int64 used_id, leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_resource_id_ <= used_id) {
    next_avail_resource_id_ = used_id + 1;
    batch->Put(kNextResIdKey, base::Int64ToString(next_avail_resource_id_));
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
  return db_ != NULL;
}

void ServiceWorkerDatabase::Disable(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK) {
    DLOG(ERROR) << "Failed at: " << from_here.ToString()
                << " with error: " << StatusToString(status);
    DLOG(ERROR) << "ServiceWorkerDatabase is disabled.";
  }
  state_ = DISABLED;
  db_.reset();
}

void ServiceWorkerDatabase::HandleOpenResult(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK)
    Disable(from_here, status);
  ServiceWorkerMetrics::CountOpenDatabaseResult(status);
}

void ServiceWorkerDatabase::HandleReadResult(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK)
    Disable(from_here, status);
  ServiceWorkerMetrics::CountReadDatabaseResult(status);
}

void ServiceWorkerDatabase::HandleWriteResult(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK)
    Disable(from_here, status);
  ServiceWorkerMetrics::CountWriteDatabaseResult(status);
}

}  // namespace content
