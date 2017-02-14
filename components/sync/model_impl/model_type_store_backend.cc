// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_backend.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/protocol/model_type_store_schema_descriptor.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using sync_pb::ModelTypeStoreSchemaDescriptor;

namespace syncer {

const int64_t kInvalidSchemaVersion = -1;
const int64_t ModelTypeStoreBackend::kLatestSchemaVersion = 1;
const char ModelTypeStoreBackend::kDBSchemaDescriptorRecordId[] =
    "_mts_schema_descriptor";
const char ModelTypeStoreBackend::kStoreInitResultHistogramName[] =
    "Sync.ModelTypeStoreInitResult";

// static
base::LazyInstance<ModelTypeStoreBackend::BackendMap>
    ModelTypeStoreBackend::backend_map_ = LAZY_INSTANCE_INITIALIZER;

namespace {

StoreInitResultForHistogram LevelDbStatusToStoreInitResult(
    const leveldb::Status& status) {
  if (status.ok())
    return STORE_INIT_RESULT_SUCCESS;
  if (status.IsNotFound())
    return STORE_INIT_RESULT_NOT_FOUND;
  if (status.IsCorruption())
    return STORE_INIT_RESULT_CORRUPTION;
  if (status.IsNotSupportedError())
    return STORE_INIT_RESULT_NOT_SUPPORTED;
  if (status.IsInvalidArgument())
    return STORE_INIT_RESULT_INVALID_ARGUMENT;
  if (status.IsIOError())
    return STORE_INIT_RESULT_IO_ERROR;
  return STORE_INIT_RESULT_UNKNOWN;
}

}  // namespace

ModelTypeStoreBackend::ModelTypeStoreBackend(const std::string& path)
    : path_(path) {}

ModelTypeStoreBackend::~ModelTypeStoreBackend() {
  backend_map_.Get().erase(path_);
}

std::unique_ptr<leveldb::Env> ModelTypeStoreBackend::CreateInMemoryEnv() {
  return base::WrapUnique(leveldb::NewMemEnv(leveldb::Env::Default()));
}

// static
scoped_refptr<ModelTypeStoreBackend> ModelTypeStoreBackend::GetOrCreateBackend(
    const std::string& path,
    std::unique_ptr<leveldb::Env> env,
    ModelTypeStore::Result* result) {
  if (backend_map_.Get().find(path) != backend_map_.Get().end()) {
    *result = ModelTypeStore::Result::SUCCESS;
    return make_scoped_refptr(backend_map_.Get()[path]);
  }

  scoped_refptr<ModelTypeStoreBackend> backend =
      new ModelTypeStoreBackend(path);

  *result = backend->Init(path, std::move(env));

  if (*result == ModelTypeStore::Result::SUCCESS) {
    backend_map_.Get()[path] = backend.get();
  } else {
    backend = nullptr;
  }

  return backend;
}

ModelTypeStore::Result ModelTypeStoreBackend::Init(
    const std::string& path,
    std::unique_ptr<leveldb::Env> env) {
  DFAKE_SCOPED_LOCK(push_pop_);

  env_ = std::move(env);

  leveldb::Status status = OpenDatabase(path, env_.get());
  if (status.IsCorruption()) {
    DCHECK(db_ == nullptr);
    status = DestroyDatabase(path, env_.get());
    if (status.ok())
      status = OpenDatabase(path, env_.get());
    if (status.ok())
      RecordStoreInitResultHistogram(
          STORE_INIT_RESULT_RECOVERED_AFTER_CORRUPTION);
  }
  if (!status.ok()) {
    DCHECK(db_ == nullptr);
    RecordStoreInitResultHistogram(LevelDbStatusToStoreInitResult(status));
    return ModelTypeStore::Result::UNSPECIFIED_ERROR;
  }

  int64_t current_version = GetStoreVersion();
  if (current_version == kInvalidSchemaVersion) {
    RecordStoreInitResultHistogram(STORE_INIT_RESULT_SCHEMA_DESCRIPTOR_ISSUE);
    return ModelTypeStore::Result::UNSPECIFIED_ERROR;
  }

  if (current_version != kLatestSchemaVersion) {
    ModelTypeStore::Result result =
        Migrate(current_version, kLatestSchemaVersion);
    if (result != ModelTypeStore::Result::SUCCESS) {
      RecordStoreInitResultHistogram(STORE_INIT_RESULT_MIGRATION);
      return result;
    }
  }
  RecordStoreInitResultHistogram(STORE_INIT_RESULT_SUCCESS);
  return ModelTypeStore::Result::SUCCESS;
}

leveldb::Status ModelTypeStoreBackend::OpenDatabase(const std::string& path,
                                                    leveldb::Env* env) {
  leveldb::DB* db_raw = nullptr;
  leveldb::Options options;
  options.create_if_missing = true;
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  options.paranoid_checks = true;
  if (env)
    options.env = env;

  leveldb::Status status = leveldb::DB::Open(options, path, &db_raw);
  DCHECK(status.ok() != (db_raw == nullptr));
  if (status.ok())
    db_.reset(db_raw);
  return status;
}

leveldb::Status ModelTypeStoreBackend::DestroyDatabase(const std::string& path,
                                                       leveldb::Env* env) {
  leveldb::Options options;
  if (env)
    options.env = env;
  return leveldb::DestroyDB(path, options);
}

ModelTypeStore::Result ModelTypeStoreBackend::ReadRecordsWithPrefix(
    const std::string& prefix,
    const ModelTypeStore::IdList& id_list,
    ModelTypeStore::RecordList* record_list,
    ModelTypeStore::IdList* missing_id_list) {
  DFAKE_SCOPED_LOCK(push_pop_);
  DCHECK(db_);
  record_list->reserve(id_list.size());
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  std::string key;
  std::string value;
  for (const std::string& id : id_list) {
    key = prefix + id;
    leveldb::Status status = db_->Get(read_options, key, &value);
    if (status.ok()) {
      record_list->emplace_back(id, value);
    } else if (status.IsNotFound()) {
      missing_id_list->push_back(id);
    } else {
      return ModelTypeStore::Result::UNSPECIFIED_ERROR;
    }
  }
  return ModelTypeStore::Result::SUCCESS;
}

ModelTypeStore::Result ModelTypeStoreBackend::ReadAllRecordsWithPrefix(
    const std::string& prefix,
    ModelTypeStore::RecordList* record_list) {
  DFAKE_SCOPED_LOCK(push_pop_);
  DCHECK(db_);
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  const leveldb::Slice prefix_slice(prefix);
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    leveldb::Slice key = iter->key();
    if (!key.starts_with(prefix_slice))
      break;
    key.remove_prefix(prefix_slice.size());
    record_list->emplace_back(key.ToString(), iter->value().ToString());
  }
  return iter->status().ok() ? ModelTypeStore::Result::SUCCESS
                             : ModelTypeStore::Result::UNSPECIFIED_ERROR;
}

ModelTypeStore::Result ModelTypeStoreBackend::WriteModifications(
    std::unique_ptr<leveldb::WriteBatch> write_batch) {
  DFAKE_SCOPED_LOCK(push_pop_);
  DCHECK(db_);
  leveldb::Status status =
      db_->Write(leveldb::WriteOptions(), write_batch.get());
  return status.ok() ? ModelTypeStore::Result::SUCCESS
                     : ModelTypeStore::Result::UNSPECIFIED_ERROR;
}

int64_t ModelTypeStoreBackend::GetStoreVersion() {
  DCHECK(db_);
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  std::string value;
  ModelTypeStoreSchemaDescriptor schema_descriptor;
  leveldb::Status status =
      db_->Get(read_options, kDBSchemaDescriptorRecordId, &value);
  if (status.IsNotFound()) {
    return 0;
  } else if (!status.ok() || !schema_descriptor.ParseFromString(value)) {
    return kInvalidSchemaVersion;
  }
  return schema_descriptor.version_number();
}

ModelTypeStore::Result ModelTypeStoreBackend::Migrate(int64_t current_version,
                                                      int64_t desired_version) {
  DCHECK(db_);
  if (current_version == 0) {
    if (Migrate0To1()) {
      current_version = 1;
    }
  }
  if (current_version == desired_version) {
    return ModelTypeStore::Result::SUCCESS;
  } else if (current_version > desired_version) {
    return ModelTypeStore::Result::SCHEMA_VERSION_TOO_HIGH;
  } else {
    return ModelTypeStore::Result::UNSPECIFIED_ERROR;
  }
}

bool ModelTypeStoreBackend::Migrate0To1() {
  DCHECK(db_);
  ModelTypeStoreSchemaDescriptor schema_descriptor;
  schema_descriptor.set_version_number(1);
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), kDBSchemaDescriptorRecordId,
               schema_descriptor.SerializeAsString());
  return status.ok();
}

// static
void ModelTypeStoreBackend::RecordStoreInitResultHistogram(
    StoreInitResultForHistogram result) {
  UMA_HISTOGRAM_ENUMERATION(kStoreInitResultHistogramName, result,
                            STORE_INIT_RESULT_COUNT);
}

}  // namespace syncer
