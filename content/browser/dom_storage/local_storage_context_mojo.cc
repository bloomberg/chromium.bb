// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_context_mojo.h"

#include <inttypes.h>
#include <cctype>  // for std::isalnum
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/leveldb/public/cpp/util.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_database.pb.h"
#include "content/browser/leveldb_wrapper_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "services/file/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "sql/connection.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace content {

// LevelDB database schema
// =======================
//
// Version 1 (in sorted order):
//   key: "VERSION"
//   value: "1"
//
//   key: "META:" + <url::Origin 'origin'>
//   value: <LocalStorageOriginMetaData serialized as a string>
//
//   key: "_" + <url::Origin> 'origin'> + '\x00' + <script controlled key>
//   value: <script controlled value>

namespace {

const char kVersionKey[] = "VERSION";
const char kOriginSeparator = '\x00';
const char kDataPrefix[] = "_";
const uint8_t kMetaPrefix[] = {'M', 'E', 'T', 'A', ':'};
const int64_t kMinSchemaVersion = 1;
const int64_t kCurrentSchemaVersion = 1;

// After this many consecutive commit errors we'll throw away the entire
// database.
const int kCommitErrorThreshold = 8;

// Limits on the cache size and number of areas in memory, over which the areas
// are purged.
#if defined(OS_ANDROID)
const unsigned kMaxStorageAreaCount = 10;
const size_t kMaxCacheSize = 2 * 1024 * 1024;
#else
const unsigned kMaxStorageAreaCount = 50;
const size_t kMaxCacheSize = 20 * 1024 * 1024;
#endif

static const uint8_t kUTF16Format = 0;
static const uint8_t kLatin1Format = 1;

std::vector<uint8_t> CreateMetaDataKey(const url::Origin& origin) {
  auto serialized_origin = leveldb::StdStringToUint8Vector(origin.Serialize());
  std::vector<uint8_t> key;
  key.reserve(arraysize(kMetaPrefix) + serialized_origin.size());
  key.insert(key.end(), kMetaPrefix, kMetaPrefix + arraysize(kMetaPrefix));
  key.insert(key.end(), serialized_origin.begin(), serialized_origin.end());
  return key;
}

void NoOpSuccess(bool success) {}
void NoOpDatabaseError(leveldb::mojom::DatabaseError error) {}

void MigrateStorageHelper(
    base::FilePath db_path,
    const scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    base::Callback<void(std::unique_ptr<LevelDBWrapperImpl::ValueMap>)>
        callback) {
  DOMStorageDatabase db(db_path);
  DOMStorageValuesMap map;
  db.ReadAllValues(&map);
  auto values = base::MakeUnique<LevelDBWrapperImpl::ValueMap>();
  for (const auto& it : map) {
    (*values)[LocalStorageContextMojo::MigrateString(it.first)] =
        LocalStorageContextMojo::MigrateString(it.second.string());
  }
  reply_task_runner->PostTask(FROM_HERE,
                              base::BindOnce(callback, base::Passed(&values)));
}

// Helper to convert from OnceCallback to Callback.
void CallMigrationCalback(LevelDBWrapperImpl::ValueMapCallback callback,
                          std::unique_ptr<LevelDBWrapperImpl::ValueMap> data) {
  std::move(callback).Run(std::move(data));
}

void AddDeleteOriginOperations(
    std::vector<leveldb::mojom::BatchedOperationPtr>* operations,
    const url::Origin& origin) {
  leveldb::mojom::BatchedOperationPtr item =
      leveldb::mojom::BatchedOperation::New();
  item->type = leveldb::mojom::BatchOperationType::DELETE_PREFIXED_KEY;
  item->key = leveldb::StdStringToUint8Vector(kDataPrefix + origin.Serialize() +
                                              kOriginSeparator);
  operations->push_back(std::move(item));

  item = leveldb::mojom::BatchedOperation::New();
  item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
  item->key = CreateMetaDataKey(origin);
  operations->push_back(std::move(item));
}

enum class CachePurgeReason {
  NotNeeded,
  SizeLimitExceeded,
  AreaCountLimitExceeded,
  InactiveOnLowEndDevice,
  AggressivePurgeTriggered
};

void RecordCachePurgedHistogram(CachePurgeReason reason,
                                size_t purged_size_kib) {
  UMA_HISTOGRAM_COUNTS_100000("LocalStorageContext.CachePurgedInKB",
                              purged_size_kib);
  switch (reason) {
    case CachePurgeReason::SizeLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.SizeLimitExceeded",
          purged_size_kib);
      break;
    case CachePurgeReason::AreaCountLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.AreaCountLimitExceeded",
          purged_size_kib);
      break;
    case CachePurgeReason::InactiveOnLowEndDevice:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.InactiveOnLowEndDevice",
          purged_size_kib);
      break;
    case CachePurgeReason::AggressivePurgeTriggered:
      UMA_HISTOGRAM_COUNTS_100000(
          "LocalStorageContext.CachePurgedInKB.AggressivePurgeTriggered",
          purged_size_kib);
      break;
    case CachePurgeReason::NotNeeded:
      NOTREACHED();
      break;
  }
}

}  // namespace

class LocalStorageContextMojo::LevelDBWrapperHolder final
    : public LevelDBWrapperImpl::Delegate {
 public:
  LevelDBWrapperHolder(LocalStorageContextMojo* context,
                       const url::Origin& origin)
      : context_(context), origin_(origin) {
    // Delay for a moment after a value is set in anticipation
    // of other values being set, so changes are batched.
    const int kCommitDefaultDelaySecs = 5;

    // To avoid excessive IO we apply limits to the amount of data being written
    // and the frequency of writes.
    const int kMaxBytesPerHour = kPerStorageAreaQuota;
    const int kMaxCommitsPerHour = 60;

    level_db_wrapper_ = base::MakeUnique<LevelDBWrapperImpl>(
        context_->database_.get(),
        kDataPrefix + origin_.Serialize() + kOriginSeparator,
        kPerStorageAreaQuota + kPerStorageAreaOverQuotaAllowance,
        base::TimeDelta::FromSeconds(kCommitDefaultDelaySecs), kMaxBytesPerHour,
        kMaxCommitsPerHour, this);
    level_db_wrapper_ptr_ = level_db_wrapper_.get();
  }

  LevelDBWrapperImpl* level_db_wrapper() { return level_db_wrapper_ptr_; }

  void OnNoBindings() override {
    has_bindings_ = false;
    // Don't delete ourselves, but do schedule an immediate commit. Possible
    // deletion will happen under memory pressure or when another localstorage
    // area is opened.
    level_db_wrapper()->ScheduleImmediateCommit();
  }

  std::vector<leveldb::mojom::BatchedOperationPtr> PrepareToCommit() override {
    std::vector<leveldb::mojom::BatchedOperationPtr> operations;

    // Write schema version if not already done so before.
    if (!context_->database_initialized_) {
      leveldb::mojom::BatchedOperationPtr item =
          leveldb::mojom::BatchedOperation::New();
      item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
      item->key = leveldb::StdStringToUint8Vector(kVersionKey);
      item->value = leveldb::StdStringToUint8Vector(
          base::Int64ToString(kCurrentSchemaVersion));
      operations.push_back(std::move(item));
      context_->database_initialized_ = true;
    }

    leveldb::mojom::BatchedOperationPtr item =
        leveldb::mojom::BatchedOperation::New();
    item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
    item->key = CreateMetaDataKey(origin_);
    if (level_db_wrapper()->empty()) {
      item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
    } else {
      item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
      LocalStorageOriginMetaData data;
      data.set_last_modified(base::Time::Now().ToInternalValue());
      data.set_size_bytes(level_db_wrapper()->bytes_used());
      item->value = leveldb::StdStringToUint8Vector(data.SerializeAsString());
    }
    operations.push_back(std::move(item));

    return operations;
  }

  void DidCommit(leveldb::mojom::DatabaseError error) override {
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.CommitResult",
                              leveldb::GetLevelDBStatusUMAValue(error),
                              leveldb_env::LEVELDB_STATUS_MAX);

    // Delete any old database that might still exist if we successfully wrote
    // data to LevelDB, and our LevelDB is actually disk backed.
    if (error == leveldb::mojom::DatabaseError::OK && !deleted_old_data_ &&
        !context_->subdirectory_.empty() && context_->task_runner_ &&
        !context_->old_localstorage_path_.empty()) {
      deleted_old_data_ = true;
      context_->task_runner_->PostShutdownBlockingTask(
          FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
          base::BindOnce(base::IgnoreResult(&sql::Connection::Delete),
                         sql_db_path()));
    }

    context_->OnCommitResult(error);
  }

  void MigrateData(LevelDBWrapperImpl::ValueMapCallback callback) override {
    if (context_->task_runner_ && !context_->old_localstorage_path_.empty()) {
      context_->task_runner_->PostShutdownBlockingTask(
          FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
          base::BindOnce(
              &MigrateStorageHelper, sql_db_path(),
              base::ThreadTaskRunnerHandle::Get(),
              base::Bind(&CallMigrationCalback, base::Passed(&callback))));
      return;
    }
    std::move(callback).Run(nullptr);
  }

  std::vector<LevelDBWrapperImpl::Change> FixUpData(
      const LevelDBWrapperImpl::ValueMap& data) override {
    std::vector<LevelDBWrapperImpl::Change> changes;
    // Chrome M61/M62 had a bug where keys that should have been encoded as
    // Latin1 were instead encoded as UTF16. Fix this by finding any 8-bit only
    // keys, and re-encode those. If two encodings of the key exist, the Latin1
    // encoded value should take precedence.
    size_t fix_count = 0;
    for (const auto& it : data) {
      // Skip over any Latin1 encoded keys, or unknown encodings/corrupted data.
      if (it.first.empty() || it.first[0] != kUTF16Format)
        continue;
      // Check if key is actually 8-bit safe.
      bool is_8bit = true;
      for (size_t i = 1; i < it.first.size(); i += sizeof(base::char16)) {
        // Don't just cast to char16* as that could be undefined behavior.
        // Instead use memcpy for the conversion, which compilers will generally
        // optimize away anyway.
        base::char16 char_val;
        memcpy(&char_val, it.first.data() + i, sizeof(base::char16));
        if (char_val & 0xff00) {
          is_8bit = false;
          break;
        }
      }
      if (!is_8bit)
        continue;
      // Found a key that should have been encoded differently. Decode and
      // re-encode.
      std::vector<uint8_t> key(1 + (it.first.size() - 1) / 2);
      key[0] = kLatin1Format;
      for (size_t in = 1, out = 1; in < it.first.size();
           in += sizeof(base::char16), out++) {
        base::char16 char_val;
        memcpy(&char_val, it.first.data() + in, sizeof(base::char16));
        key[out] = char_val;
      }
      // Delete incorrect key.
      changes.push_back(std::make_pair(it.first, base::nullopt));
      fix_count++;
      // Check if correct key already exists in data.
      auto new_it = data.find(key);
      if (new_it != data.end())
        continue;
      // Update value for correct key.
      changes.push_back(std::make_pair(key, it.second));
    }
    UMA_HISTOGRAM_BOOLEAN("LocalStorageContext.MigrationFixUpNeeded",
                          fix_count != 0);
    return changes;
  }

  void OnMapLoaded(leveldb::mojom::DatabaseError error) override {
    if (error != leveldb::mojom::DatabaseError::OK)
      UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.MapLoadError",
                                leveldb::GetLevelDBStatusUMAValue(error),
                                leveldb_env::LEVELDB_STATUS_MAX);
  }

  void Bind(mojom::LevelDBWrapperRequest request) {
    has_bindings_ = true;
    level_db_wrapper()->Bind(std::move(request));
  }

  bool has_bindings() const { return has_bindings_; }

 private:
  base::FilePath sql_db_path() const {
    if (context_->old_localstorage_path_.empty())
      return base::FilePath();
    return context_->old_localstorage_path_.Append(
        DOMStorageArea::DatabaseFileNameFromOrigin(origin_.GetURL()));
  }

  LocalStorageContextMojo* context_;
  url::Origin origin_;
  std::unique_ptr<LevelDBWrapperImpl> level_db_wrapper_;
  // Holds the same value as |level_db_wrapper_|. The reason for this is that
  // during destruction of the LevelDBWrapperImpl instance we might still get
  // called and need access  to the LevelDBWrapperImpl instance. The unique_ptr
  // could already be null, but this field should still be valid.
  LevelDBWrapperImpl* level_db_wrapper_ptr_;
  bool deleted_old_data_ = false;
  bool has_bindings_ = false;
};

LocalStorageContextMojo::LocalStorageContextMojo(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    service_manager::Connector* connector,
    scoped_refptr<DOMStorageTaskRunner> legacy_task_runner,
    const base::FilePath& old_localstorage_path,
    const base::FilePath& subdirectory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : connector_(connector ? connector->Clone() : nullptr),
      subdirectory_(subdirectory),
      special_storage_policy_(std::move(special_storage_policy)),
      memory_dump_id_(base::StringPrintf("LocalStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      task_runner_(std::move(legacy_task_runner)),
      old_localstorage_path_(old_localstorage_path),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      weak_ptr_factory_(this) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "LocalStorage", task_runner);
}

void LocalStorageContextMojo::OpenLocalStorage(
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
  RunWhenConnected(base::BindOnce(&LocalStorageContextMojo::BindLocalStorage,
                                  weak_ptr_factory_.GetWeakPtr(), origin,
                                  std::move(request)));
}

void LocalStorageContextMojo::GetStorageUsage(
    GetStorageUsageCallback callback) {
  RunWhenConnected(
      base::BindOnce(&LocalStorageContextMojo::RetrieveStorageUsage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalStorageContextMojo::DeleteStorage(const url::Origin& origin) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageContextMojo::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), origin));
    return;
  }

  auto found = level_db_wrappers_.find(origin);
  if (found != level_db_wrappers_.end()) {
    // Renderer process expects |source| to always be two newline separated
    // strings.
    found->second->level_db_wrapper()->DeleteAll("\n",
                                                 base::BindOnce(&NoOpSuccess));
    found->second->level_db_wrapper()->ScheduleImmediateCommit();
  } else if (database_) {
    std::vector<leveldb::mojom::BatchedOperationPtr> operations;
    AddDeleteOriginOperations(&operations, origin);
    database_->Write(std::move(operations), base::BindOnce(&NoOpDatabaseError));
  }
}

void LocalStorageContextMojo::DeleteStorageForPhysicalOrigin(
    const url::Origin& origin) {
  GetStorageUsage(base::BindOnce(
      &LocalStorageContextMojo::OnGotStorageUsageForDeletePhysicalOrigin,
      weak_ptr_factory_.GetWeakPtr(), origin));
}

void LocalStorageContextMojo::Flush() {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageContextMojo::Flush,
                                    weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  for (const auto& it : level_db_wrappers_)
    it.second->level_db_wrapper()->ScheduleImmediateCommit();
}

void LocalStorageContextMojo::FlushOriginForTesting(const url::Origin& origin) {
  if (connection_state_ != CONNECTION_FINISHED)
    return;
  const auto& it = level_db_wrappers_.find(origin);
  if (it == level_db_wrappers_.end())
    return;
  it->second->level_db_wrapper()->ScheduleImmediateCommit();
}

void LocalStorageContextMojo::ShutdownAndDelete() {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ != CONNECTION_FINISHED) {
    connection_state_ = CONNECTION_SHUTDOWN;
    OnShutdownComplete(leveldb::mojom::DatabaseError::OK);
    return;
  }

  connection_state_ = CONNECTION_SHUTDOWN;

  // Flush any uncommitted data.
  for (const auto& it : level_db_wrappers_)
    it.second->level_db_wrapper()->ScheduleImmediateCommit();

  // Respect the content policy settings about what to
  // keep and what to discard.
  if (force_keep_session_state_) {
    OnShutdownComplete(leveldb::mojom::DatabaseError::OK);
    return;  // Keep everything.
  }

  bool has_session_only_origins =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  if (has_session_only_origins) {
    RetrieveStorageUsage(
        base::BindOnce(&LocalStorageContextMojo::OnGotStorageUsageForShutdown,
                       base::Unretained(this)));
  } else {
    OnShutdownComplete(leveldb::mojom::DatabaseError::OK);
  }
}

void LocalStorageContextMojo::PurgeMemory() {
  size_t total_cache_size, unused_wrapper_count;
  GetStatistics(&total_cache_size, &unused_wrapper_count);

  for (auto it = level_db_wrappers_.begin(); it != level_db_wrappers_.end();) {
    if (it->second->has_bindings()) {
      it->second->level_db_wrapper()->PurgeMemory();
      ++it;
    } else {
      it = level_db_wrappers_.erase(it);
    }
  }

  // Track the size of cache purged.
  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_wrapper_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordCachePurgedHistogram(CachePurgeReason::AggressivePurgeTriggered,
                             purged_size_kib);
}

void LocalStorageContextMojo::PurgeUnusedWrappersIfNeeded() {
  size_t total_cache_size, unused_wrapper_count;
  GetStatistics(&total_cache_size, &unused_wrapper_count);

  // Nothing to purge.
  if (!unused_wrapper_count)
    return;

  CachePurgeReason purge_reason = CachePurgeReason::NotNeeded;

  if (total_cache_size > kMaxCacheSize)
    purge_reason = CachePurgeReason::SizeLimitExceeded;
  else if (level_db_wrappers_.size() > kMaxStorageAreaCount)
    purge_reason = CachePurgeReason::AreaCountLimitExceeded;
  else if (is_low_end_device_)
    purge_reason = CachePurgeReason::InactiveOnLowEndDevice;

  if (purge_reason == CachePurgeReason::NotNeeded)
    return;

  for (auto it = level_db_wrappers_.begin(); it != level_db_wrappers_.end();) {
    if (it->second->has_bindings())
      ++it;
    else
      it = level_db_wrappers_.erase(it);
  }

  // Track the size of cache purged.
  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_wrapper_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordCachePurgedHistogram(purge_reason, purged_size_kib);
}

void LocalStorageContextMojo::SetDatabaseForTesting(
    leveldb::mojom::LevelDBDatabaseAssociatedPtr database) {
  DCHECK_EQ(connection_state_, NO_CONNECTION);
  connection_state_ = CONNECTION_IN_PROGRESS;
  database_ = std::move(database);
  OnDatabaseOpened(true, leveldb::mojom::DatabaseError::OK);
}

bool LocalStorageContextMojo::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (connection_state_ != CONNECTION_FINISHED)
    return true;

  std::string context_name =
      base::StringPrintf("site_storage/localstorage_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));

  // Account for leveldb memory usage, which actually lives in the file service.
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(memory_dump_id_);
  // The size of the leveldb dump will be added by the leveldb service.
  auto* leveldb_mad = pmd->CreateAllocatorDump(context_name + "/leveldb");
  // Specifies that the current context is responsible for keeping memory alive.
  int kImportance = 2;
  pmd->AddOwnershipEdge(leveldb_mad->guid(), global_dump->guid(), kImportance);

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    size_t total_cache_size, unused_wrapper_count;
    GetStatistics(&total_cache_size, &unused_wrapper_count);
    auto* mad = pmd->CreateAllocatorDump(context_name + "/cache_size");
    mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                   base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                   total_cache_size);
    mad->AddScalar("total_areas",
                   base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                   level_db_wrappers_.size());
    return true;
  }
  for (const auto& it : level_db_wrappers_) {
    // Limit the url length to 50 and strip special characters.
    std::string url = it.first.Serialize().substr(0, 50);
    for (size_t index = 0; index < url.size(); ++index) {
      if (!std::isalnum(url[index]))
        url[index] = '_';
    }
    std::string wrapper_dump_name = base::StringPrintf(
        "%s/%s/0x%" PRIXPTR, context_name.c_str(), url.c_str(),
        reinterpret_cast<uintptr_t>(it.second->level_db_wrapper()));
    it.second->level_db_wrapper()->OnMemoryDump(wrapper_dump_name, pmd);
  }
  return true;
}

// static
std::vector<uint8_t> LocalStorageContextMojo::MigrateString(
    const base::string16& input) {
  // TODO(mek): Deduplicate this somehow with the code in
  // LocalStorageCachedArea::String16ToUint8Vector.
  bool is_8bit = true;
  for (const auto& c : input) {
    if (c & 0xff00) {
      is_8bit = false;
      break;
    }
  }
  if (is_8bit) {
    std::vector<uint8_t> result(input.size() + 1);
    result[0] = kLatin1Format;
    std::copy(input.begin(), input.end(), result.begin() + 1);
    return result;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
  std::vector<uint8_t> result;
  result.reserve(input.size() * sizeof(base::char16) + 1);
  result.push_back(kUTF16Format);
  result.insert(result.end(), data, data + input.size() * sizeof(base::char16));
  return result;
}

LocalStorageContextMojo::~LocalStorageContextMojo() {
  DCHECK_EQ(connection_state_, CONNECTION_SHUTDOWN);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void LocalStorageContextMojo::RunWhenConnected(base::OnceClosure callback) {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // If we don't have a filesystem_connection_, we'll need to establish one.
  if (connection_state_ == NO_CONNECTION) {
    connection_state_ = CONNECTION_IN_PROGRESS;
    InitiateConnection();
  }

  if (connection_state_ == CONNECTION_IN_PROGRESS) {
    // Queue this OpenLocalStorage call for when we have a level db pointer.
    on_database_opened_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}

void LocalStorageContextMojo::InitiateConnection(bool in_memory_only) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);
  // Unit tests might not always have a Connector, use in-memory only if that
  // happens.
  if (!connector_) {
    OnDatabaseOpened(false, leveldb::mojom::DatabaseError::OK);
    return;
  }

  if (!subdirectory_.empty() && !in_memory_only) {
    // We were given a subdirectory to write to. Get it and use a disk backed
    // database.
    connector_->BindInterface(file::mojom::kServiceName, &file_system_);
    file_system_->GetSubDirectory(
        subdirectory_.AsUTF8Unsafe(), MakeRequest(&directory_),
        base::BindOnce(&LocalStorageContextMojo::OnDirectoryOpened,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // We were not given a subdirectory. Use a memory backed database.
    connector_->BindInterface(file::mojom::kServiceName, &leveldb_service_);
    leveldb_service_->OpenInMemory(
        memory_dump_id_, MakeRequest(&database_),
        base::BindOnce(&LocalStorageContextMojo::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr(), true));
  }
}

void LocalStorageContextMojo::OnDirectoryOpened(
    filesystem::mojom::FileError err) {
  if (err != filesystem::mojom::FileError::OK) {
    // We failed to open the directory; continue with startup so that we create
    // the |level_db_wrappers_|.
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DirectoryOpenError",
                              -static_cast<base::File::Error>(err),
                              -base::File::FILE_ERROR_MAX);
    LogDatabaseOpenResult(OpenResult::DIRECTORY_OPEN_FAILED);
    OnDatabaseOpened(false, leveldb::mojom::DatabaseError::OK);
    return;
  }

  // Now that we have a directory, connect to the LevelDB service and get our
  // database.
  connector_->BindInterface(file::mojom::kServiceName, &leveldb_service_);

  // We might still need to use the directory, so create a clone.
  filesystem::mojom::DirectoryPtr directory_clone;
  directory_->Clone(MakeRequest(&directory_clone));

  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // use minimum
  // Default write_buffer_size is 4 MB but that might leave a 3.999
  // memory allocation in RAM from a log file recovery.
  options.write_buffer_size = 64 * 1024;
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();
  leveldb_service_->OpenWithOptions(
      std::move(options), std::move(directory_clone), "leveldb",
      memory_dump_id_, MakeRequest(&database_),
      base::BindOnce(&LocalStorageContextMojo::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr(), false));
}

void LocalStorageContextMojo::OnDatabaseOpened(
    bool in_memory,
    leveldb::mojom::DatabaseError status) {
  if (status != leveldb::mojom::DatabaseError::OK) {
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DatabaseOpenError",
                              leveldb::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    if (in_memory) {
      UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DatabaseOpenError.Memory",
                                leveldb::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DatabaseOpenError.Disk",
                                leveldb::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    }
    LogDatabaseOpenResult(OpenResult::DATABASE_OPEN_FAILED);
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase("LocalStorageContext.OpenResultAfterOpenFailed");
    return;
  }

  // Verify DB schema version.
  if (database_) {
    database_->Get(
        leveldb::StdStringToUint8Vector(kVersionKey),
        base::BindOnce(&LocalStorageContextMojo::OnGotDatabaseVersion,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  OnConnectionFinished();
}

void LocalStorageContextMojo::OnGotDatabaseVersion(
    leveldb::mojom::DatabaseError status,
    const std::vector<uint8_t>& value) {
  if (status == leveldb::mojom::DatabaseError::NOT_FOUND) {
    // New database, nothing more to do. Current version will get written
    // when first data is committed.
  } else if (status == leveldb::mojom::DatabaseError::OK) {
    // Existing database, check if version number matches current schema
    // version.
    int64_t db_version;
    if (!base::StringToInt64(leveldb::Uint8VectorToStdString(value),
                             &db_version) ||
        db_version < kMinSchemaVersion || db_version > kCurrentSchemaVersion) {
      LogDatabaseOpenResult(OpenResult::INVALID_VERSION);
      DeleteAndRecreateDatabase(
          "LocalStorageContext.OpenResultAfterInvalidVersion");
      return;
    }

    database_initialized_ = true;
  } else {
    // Other read error. Possibly database corruption.
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.ReadVersionError",
                              leveldb::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    LogDatabaseOpenResult(OpenResult::VERSION_READ_ERROR);
    DeleteAndRecreateDatabase(
        "LocalStorageContext.OpenResultAfterReadVersionError");
    return;
  }

  OnConnectionFinished();
}

void LocalStorageContextMojo::OnConnectionFinished() {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);
  if (!database_) {
    directory_.reset();
    file_system_.reset();
    leveldb_service_.reset();
  }

  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

  open_result_histogram_ = nullptr;

  // |database_| should be known to either be valid or invalid by now. Run our
  // delayed bindings.
  connection_state_ = CONNECTION_FINISHED;
  for (size_t i = 0; i < on_database_opened_callbacks_.size(); ++i)
    std::move(on_database_opened_callbacks_[i]).Run();
  on_database_opened_callbacks_.clear();
}

void LocalStorageContextMojo::DeleteAndRecreateDatabase(
    const char* histogram_name) {
  // We're about to set database_ to null, so delete and LevelDBWrappers
  // that might still be using the old database.
  level_db_wrappers_.clear();

  // Reset state to be in process of connecting. This will cause requests for
  // LevelDBWrappers to be queued until the connection is complete.
  connection_state_ = CONNECTION_IN_PROGRESS;
  commit_error_count_ = 0;
  database_ = nullptr;
  open_result_histogram_ = histogram_name;

  bool recreate_in_memory = false;

  // If tried to recreate database on disk already, try again but this time
  // in memory.
  if (tried_to_recreate_during_open_ && !subdirectory_.empty()) {
    recreate_in_memory = true;
  } else if (tried_to_recreate_during_open_) {
    // Give up completely, run without any database.
    OnConnectionFinished();
    return;
  }

  tried_to_recreate_during_open_ = true;

  // Unit tests might not have a bound file_service_, in which case there is
  // nothing to retry.
  if (!file_system_.is_bound()) {
    OnConnectionFinished();
    return;
  }

  // Destroy database, and try again.
  if (directory_.is_bound()) {
    leveldb_service_->Destroy(
        std::move(directory_), "leveldb",
        base::BindOnce(&LocalStorageContextMojo::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void LocalStorageContextMojo::OnDBDestroyed(
    bool recreate_in_memory,
    leveldb::mojom::DatabaseError status) {
  UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.DestroyDBResult",
                            leveldb::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
}

// The (possibly delayed) implementation of OpenLocalStorage(). Can be called
// directly from that function, or through |on_database_open_callbacks_|.
void LocalStorageContextMojo::BindLocalStorage(
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
  GetOrCreateDBWrapper(origin)->Bind(std::move(request));
}

LocalStorageContextMojo::LevelDBWrapperHolder*
LocalStorageContextMojo::GetOrCreateDBWrapper(const url::Origin& origin) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  auto found = level_db_wrappers_.find(origin);
  if (found != level_db_wrappers_.end()) {
    return found->second.get();
  }

  size_t total_cache_size, unused_wrapper_count;
  GetStatistics(&total_cache_size, &unused_wrapper_count);

  // Track the total localStorage cache size.
  UMA_HISTOGRAM_COUNTS_100000("LocalStorageContext.CacheSizeInKB",
                              total_cache_size / 1024);

  PurgeUnusedWrappersIfNeeded();

  auto holder = base::MakeUnique<LevelDBWrapperHolder>(this, origin);
  LevelDBWrapperHolder* holder_ptr = holder.get();
  level_db_wrappers_[origin] = std::move(holder);
  return holder_ptr;
}

void LocalStorageContextMojo::RetrieveStorageUsage(
    GetStorageUsageCallback callback) {
  if (!database_) {
    // If for whatever reason no leveldb database is available, no storage is
    // used, so return an array only containing the current leveldb wrappers.
    std::vector<LocalStorageUsageInfo> result;
    base::Time now = base::Time::Now();
    for (const auto& it : level_db_wrappers_) {
      LocalStorageUsageInfo info;
      info.origin = it.first.GetURL();
      info.last_modified = now;
      result.push_back(std::move(info));
    }
    std::move(callback).Run(std::move(result));
    return;
  }

  database_->GetPrefixed(
      std::vector<uint8_t>(kMetaPrefix, kMetaPrefix + arraysize(kMetaPrefix)),
      base::BindOnce(&LocalStorageContextMojo::OnGotMetaData,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void LocalStorageContextMojo::OnGotMetaData(
    GetStorageUsageCallback callback,
    leveldb::mojom::DatabaseError status,
    std::vector<leveldb::mojom::KeyValuePtr> data) {
  std::vector<LocalStorageUsageInfo> result;
  std::set<url::Origin> origins;
  for (const auto& row : data) {
    DCHECK_GT(row->key.size(), arraysize(kMetaPrefix));
    LocalStorageUsageInfo info;
    info.origin = GURL(leveldb::Uint8VectorToStdString(row->key).substr(
        arraysize(kMetaPrefix)));
    origins.insert(url::Origin(info.origin));
    if (!info.origin.is_valid()) {
      // TODO(mek): Deal with database corruption.
      continue;
    }

    LocalStorageOriginMetaData data;
    if (!data.ParseFromArray(row->value.data(), row->value.size())) {
      // TODO(mek): Deal with database corruption.
      continue;
    }
    info.data_size = data.size_bytes();
    info.last_modified = base::Time::FromInternalValue(data.last_modified());
    result.push_back(std::move(info));
  }
  // Add any origins for which LevelDBWrappers exist, but which haven't
  // committed any data to disk yet.
  base::Time now = base::Time::Now();
  for (const auto& it : level_db_wrappers_) {
    if (origins.find(it.first) != origins.end())
      continue;
    // Skip any origins that definitely don't have any data.
    if (!it.second->level_db_wrapper()->has_pending_load_tasks() &&
        it.second->level_db_wrapper()->empty()) {
      continue;
    }
    LocalStorageUsageInfo info;
    info.origin = it.first.GetURL();
    info.last_modified = now;
    result.push_back(std::move(info));
  }
  std::move(callback).Run(std::move(result));
}

void LocalStorageContextMojo::OnGotStorageUsageForDeletePhysicalOrigin(
    const url::Origin& origin,
    std::vector<LocalStorageUsageInfo> usage) {
  for (const auto& info : usage) {
    url::Origin origin_candidate(info.origin);
    if (!origin_candidate.IsSameOriginWith(origin) &&
        origin_candidate.IsSamePhysicalOriginWith(origin))
      DeleteStorage(origin_candidate);
  }
  DeleteStorage(origin);
}

void LocalStorageContextMojo::OnGotStorageUsageForShutdown(
    std::vector<LocalStorageUsageInfo> usage) {
  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  for (const auto& info : usage) {
    if (special_storage_policy_->IsStorageProtected(info.origin))
      continue;
    if (!special_storage_policy_->IsStorageSessionOnly(info.origin))
      continue;

    AddDeleteOriginOperations(&operations, url::Origin(info.origin));
  }

  if (!operations.empty()) {
    database_->Write(
        std::move(operations),
        base::BindOnce(&LocalStorageContextMojo::OnShutdownComplete,
                       base::Unretained(this)));
  } else {
    OnShutdownComplete(leveldb::mojom::DatabaseError::OK);
  }
}

void LocalStorageContextMojo::OnShutdownComplete(
    leveldb::mojom::DatabaseError error) {
  delete this;
}

void LocalStorageContextMojo::GetStatistics(size_t* total_cache_size,
                                            size_t* unused_wrapper_count) {
  *total_cache_size = 0;
  *unused_wrapper_count = 0;
  for (const auto& it : level_db_wrappers_) {
    *total_cache_size += it.second->level_db_wrapper()->bytes_used();
    if (!it.second->has_bindings())
      (*unused_wrapper_count)++;
  }
}

void LocalStorageContextMojo::OnCommitResult(
    leveldb::mojom::DatabaseError error) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  if (error == leveldb::mojom::DatabaseError::OK) {
    commit_error_count_ = 0;
    return;
  }

  commit_error_count_++;
  if (commit_error_count_ > kCommitErrorThreshold) {
    if (tried_to_recover_from_commit_errors_) {
      // We already tried to recover from a high commit error rate before, but
      // are still having problems: there isn't really anything left to try, so
      // just ignore errors.
      return;
    }
    tried_to_recover_from_commit_errors_ = true;

    // Deleting LevelDBWrappers in here could cause more commits (and commit
    // errors), but those commits won't reach OnCommitResult because the wrapper
    // will have been deleted before the commit finishes.
    DeleteAndRecreateDatabase(
        "LocalStorageContext.OpenResultAfterCommitErrors");
  }
}

void LocalStorageContextMojo::LogDatabaseOpenResult(OpenResult result) {
  if (result != OpenResult::SUCCESS) {
    UMA_HISTOGRAM_ENUMERATION("LocalStorageContext.OpenError", result,
                              OpenResult::MAX);
  }
  if (open_result_histogram_) {
    base::UmaHistogramEnumeration(open_result_histogram_, result,
                                  OpenResult::MAX);
  }
}

}  // namespace content
