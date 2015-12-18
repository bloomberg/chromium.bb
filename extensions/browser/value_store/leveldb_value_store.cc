// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/leveldb_value_store.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using content::BrowserThread;

namespace {

const char kInvalidJson[] = "Invalid JSON";
const char kCannotSerialize[] = "Cannot serialize value to JSON";
const char kRestoredDuringOpen[] = "Database corruption repaired during open";

// UMA values used when recovering from a corrupted leveldb.
// Do not change/delete these values as you will break reporting for older
// copies of Chrome. Only add new values to the end.
enum LevelDBCorruptionRecoveryValue {
  LEVELDB_RESTORE_DELETE_SUCCESS = 0,
  LEVELDB_RESTORE_DELETE_FAILURE,
  LEVELDB_RESTORE_REPAIR_SUCCESS,
  LEVELDB_RESTORE_MAX
};

// Scoped leveldb snapshot which releases the snapshot on destruction.
class ScopedSnapshot {
 public:
  explicit ScopedSnapshot(leveldb::DB* db)
      : db_(db), snapshot_(db->GetSnapshot()) {}

  ~ScopedSnapshot() {
    db_->ReleaseSnapshot(snapshot_);
  }

  const leveldb::Snapshot* get() {
    return snapshot_;
  }

 private:
  leveldb::DB* db_;
  const leveldb::Snapshot* snapshot_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSnapshot);
};

ValueStore::StatusCode LevelDbToValueStoreStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return ValueStore::OK;
  if (status.IsCorruption())
    return ValueStore::CORRUPTION;
  return ValueStore::OTHER_ERROR;
}

}  // namespace

LeveldbValueStore::LeveldbValueStore(const std::string& uma_client_name,
                                     const base::FilePath& db_path)
    : db_path_(db_path),
      db_unrecoverable_(false),
      open_histogram_(nullptr),
      restore_histogram_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  open_options_.max_open_files = 0;  // Use minimum.
  open_options_.create_if_missing = true;
  open_options_.paranoid_checks = true;
  open_options_.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;

  read_options_.verify_checksums = true;

  // Used in lieu of UMA_HISTOGRAM_ENUMERATION because the histogram name is
  // not a constant.
  open_histogram_ = base::LinearHistogram::FactoryGet(
      "Extensions.Database.Open." + uma_client_name, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  restore_histogram_ = base::LinearHistogram::FactoryGet(
      "Extensions.Database.Restore." + uma_client_name, 1, LEVELDB_RESTORE_MAX,
      LEVELDB_RESTORE_MAX + 1, base::Histogram::kUmaTargetedHistogramFlag);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "LeveldbValueStore", base::ThreadTaskRunnerHandle::Get());
}

LeveldbValueStore::~LeveldbValueStore() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  // Delete the database from disk if it's empty (but only if we managed to
  // open it!). This is safe on destruction, assuming that we have exclusive
  // access to the database.
  if (db_ && IsEmpty())
    DeleteDbFile();
}

size_t LeveldbValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t LeveldbValueStore::GetBytesInUse(
    const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t LeveldbValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

ValueStore::ReadResult LeveldbValueStore::Get(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return MakeReadResult(status);

  scoped_ptr<base::Value> setting;
  status.Merge(ReadFromDb(key, &setting));
  if (!status.ok())
    return MakeReadResult(status);

  base::DictionaryValue* settings = new base::DictionaryValue();
  if (setting)
    settings->SetWithoutPathExpansion(key, setting.release());
  return MakeReadResult(make_scoped_ptr(settings), status);
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return MakeReadResult(status);

  scoped_ptr<base::DictionaryValue> settings(new base::DictionaryValue());

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<base::Value> setting;
    status.Merge(ReadFromDb(*it, &setting));
    if (!status.ok())
      return MakeReadResult(status);
    if (setting)
      settings->SetWithoutPathExpansion(*it, setting.release());
  }

  return MakeReadResult(std::move(settings), status);
}

ValueStore::ReadResult LeveldbValueStore::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return MakeReadResult(status);

  base::JSONReader json_reader;
  leveldb::ReadOptions options = leveldb::ReadOptions();
  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  scoped_ptr<base::DictionaryValue> settings(new base::DictionaryValue());

  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    scoped_ptr<base::Value> value =
        json_reader.ReadToValue(it->value().ToString());
    if (!value) {
      return MakeReadResult(Status(
          CORRUPTION,
          Delete(key).ok() ? RESTORE_REPAIR_SUCCESS : RESTORE_DELETE_FAILURE,
          kInvalidJson));
    }
    settings->SetWithoutPathExpansion(key, std::move(value));
  }

  if (it->status().IsNotFound()) {
    NOTREACHED() << "IsNotFound() but iterating over all keys?!";
    return MakeReadResult(std::move(settings), status);
  }

  if (!it->status().ok()) {
    status.Merge(ToValueStoreError(it->status()));
    return MakeReadResult(status);
  }

  return MakeReadResult(std::move(settings), status);
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return MakeWriteResult(status);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  status.Merge(AddToBatch(options, key, value, &batch, changes.get()));
  if (!status.ok())
    return MakeWriteResult(status);

  status.Merge(WriteToDb(&batch));
  return status.ok() ? MakeWriteResult(std::move(changes), status)
                     : MakeWriteResult(status);
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const base::DictionaryValue& settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return MakeWriteResult(status);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (base::DictionaryValue::Iterator it(settings);
       !it.IsAtEnd(); it.Advance()) {
    status.Merge(
        AddToBatch(options, it.key(), it.value(), &batch, changes.get()));
    if (!status.ok())
      return MakeWriteResult(status);
  }

  status.Merge(WriteToDb(&batch));
  return status.ok() ? MakeWriteResult(std::move(changes), status)
                     : MakeWriteResult(status);
}

ValueStore::WriteResult LeveldbValueStore::Remove(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status status = EnsureDbIsOpen();
  if (!status.ok())
    return MakeWriteResult(status);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<base::Value> old_value;
    status.Merge(ReadFromDb(*it, &old_value));
    if (!status.ok())
      return MakeWriteResult(status);

    if (old_value) {
      changes->push_back(ValueStoreChange(*it, old_value.release(), NULL));
      batch.Delete(*it);
    }
  }

  leveldb::Status ldb_status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!ldb_status.ok() && !ldb_status.IsNotFound()) {
    status.Merge(ToValueStoreError(ldb_status));
    return MakeWriteResult(status);
  }
  return MakeWriteResult(std::move(changes), status);
}

ValueStore::WriteResult LeveldbValueStore::Clear() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  ReadResult read_result = Get();
  if (!read_result->status().ok())
    return MakeWriteResult(read_result->status());

  base::DictionaryValue& whole_db = read_result->settings();
  while (!whole_db.empty()) {
    std::string next_key = base::DictionaryValue::Iterator(whole_db).key();
    scoped_ptr<base::Value> next_value;
    whole_db.RemoveWithoutPathExpansion(next_key, &next_value);
    changes->push_back(ValueStoreChange(next_key, next_value.release(), NULL));
  }

  DeleteDbFile();
  return MakeWriteResult(std::move(changes), read_result->status());
}

bool LeveldbValueStore::WriteToDbForTest(leveldb::WriteBatch* batch) {
  return WriteToDb(batch).ok();
}

bool LeveldbValueStore::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Return true so that the provider is not disabled.
  if (!db_)
    return true;

  std::string value;
  uint64 size;
  bool res = db_->GetProperty("leveldb.approximate-memory-usage", &value);
  DCHECK(res);
  res = base::StringToUint64(value, &size);
  DCHECK(res);

  auto dump = pmd->CreateAllocatorDump(
      base::StringPrintf("leveldb/value_store/%s/%p",
                         open_histogram_->histogram_name().c_str(), this));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, size);

  // Memory is allocated from system allocator (malloc).
  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name)
    pmd->AddSuballocation(dump->guid(), system_allocator_name);

  return true;
}

ValueStore::BackingStoreRestoreStatus LeveldbValueStore::LogRestoreStatus(
    BackingStoreRestoreStatus restore_status) {
  switch (restore_status) {
    case RESTORE_NONE:
      NOTREACHED();
      break;
    case RESTORE_DELETE_SUCCESS:
      restore_histogram_->Add(LEVELDB_RESTORE_DELETE_SUCCESS);
      break;
    case RESTORE_DELETE_FAILURE:
      restore_histogram_->Add(LEVELDB_RESTORE_DELETE_FAILURE);
      break;
    case RESTORE_REPAIR_SUCCESS:
      restore_histogram_->Add(LEVELDB_RESTORE_REPAIR_SUCCESS);
      break;
  }
  return restore_status;
}

ValueStore::BackingStoreRestoreStatus LeveldbValueStore::FixCorruption(
    const std::string* key) {
  leveldb::Status s;
  if (key && db_) {
    s = Delete(*key);
    // Deleting involves writing to the log, so it's possible to have a
    // perfectly OK database but still have a delete fail.
    if (s.ok())
      return LogRestoreStatus(RESTORE_REPAIR_SUCCESS);
    else if (s.IsIOError())
      return LogRestoreStatus(RESTORE_DELETE_FAILURE);
    // Any other kind of failure triggers a db repair.
  }

  // Make sure database is closed.
  db_.reset();

  // First try the less lossy repair.
  BackingStoreRestoreStatus restore_status = RESTORE_NONE;

  leveldb::Options repair_options;
  repair_options.create_if_missing = true;
  repair_options.paranoid_checks = true;

  // RepairDB can drop an unbounded number of leveldb tables (key/value sets).
  s = leveldb::RepairDB(db_path_.AsUTF8Unsafe(), repair_options);

  leveldb::DB* db = nullptr;
  if (s.ok()) {
    restore_status = RESTORE_REPAIR_SUCCESS;
    s = leveldb::DB::Open(open_options_, db_path_.AsUTF8Unsafe(), &db);
  }

  if (!s.ok()) {
    if (DeleteDbFile()) {
      restore_status = RESTORE_DELETE_SUCCESS;
      s = leveldb::DB::Open(open_options_, db_path_.AsUTF8Unsafe(), &db);
    } else {
      restore_status = RESTORE_DELETE_FAILURE;
    }
  }

  if (s.ok())
    db_.reset(db);
  else
    db_unrecoverable_ = true;

  if (s.ok() && key) {
    s = Delete(*key);
    if (s.ok()) {
      restore_status = RESTORE_REPAIR_SUCCESS;
    } else if (s.IsIOError()) {
      restore_status = RESTORE_DELETE_FAILURE;
    } else {
      db_.reset(db);
      if (!DeleteDbFile())
        db_unrecoverable_ = true;
      restore_status = RESTORE_DELETE_FAILURE;
    }
  }

  // Only log for the final and most extreme form of database restoration.
  LogRestoreStatus(restore_status);

  return restore_status;
}

ValueStore::Status LeveldbValueStore::EnsureDbIsOpen() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (db_)
    return Status();

  if (db_unrecoverable_) {
    return ValueStore::Status(ValueStore::CORRUPTION,
                              ValueStore::RESTORE_DELETE_FAILURE,
                              "Database corrupted");
  }

  leveldb::DB* db = NULL;
  leveldb::Status ldb_status =
      leveldb::DB::Open(open_options_, db_path_.AsUTF8Unsafe(), &db);
  open_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(ldb_status));
  Status status = ToValueStoreError(ldb_status);
  if (ldb_status.ok()) {
    db_.reset(db);
  } else if (ldb_status.IsCorruption()) {
    status.restore_status = FixCorruption(nullptr);
    if (status.restore_status != RESTORE_DELETE_FAILURE) {
      status.code = OK;
      status.message = kRestoredDuringOpen;
    }
  }

  return status;
}

ValueStore::Status LeveldbValueStore::ReadFromDb(
    const std::string& key,
    scoped_ptr<base::Value>* setting) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(setting);

  std::string value_as_json;
  leveldb::Status s = db_->Get(read_options_, key, &value_as_json);

  if (s.IsNotFound()) {
    // Despite there being no value, it was still a success. Check this first
    // because ok() is false on IsNotFound.
    return Status();
  }

  if (!s.ok())
    return ToValueStoreError(s);

  scoped_ptr<base::Value> value = base::JSONReader().ReadToValue(value_as_json);
  if (!value)
    return Status(CORRUPTION, FixCorruption(&key), kInvalidJson);

  *setting = std::move(value);
  return Status();
}

ValueStore::Status LeveldbValueStore::AddToBatch(
    ValueStore::WriteOptions options,
    const std::string& key,
    const base::Value& value,
    leveldb::WriteBatch* batch,
    ValueStoreChangeList* changes) {
  bool write_new_value = true;

  if (!(options & NO_GENERATE_CHANGES)) {
    scoped_ptr<base::Value> old_value;
    Status status = ReadFromDb(key, &old_value);
    if (!status.ok())
      return status;
    if (!old_value || !old_value->Equals(&value)) {
      changes->push_back(
          ValueStoreChange(key, old_value.release(), value.DeepCopy()));
    } else {
      write_new_value = false;
    }
  }

  if (write_new_value) {
    std::string value_as_json;
    if (!base::JSONWriter::Write(value, &value_as_json))
      return Status(OTHER_ERROR, kCannotSerialize);
    batch->Put(key, value_as_json);
  }

  return Status();
}

ValueStore::Status LeveldbValueStore::WriteToDb(leveldb::WriteBatch* batch) {
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch);
  return ToValueStoreError(status);
}

bool LeveldbValueStore::IsEmpty() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));

  it->SeekToFirst();
  bool is_empty = !it->Valid();
  if (!it->status().ok()) {
    LOG(ERROR) << "Checking DB emptiness failed: " << it->status().ToString();
    return false;
  }
  return is_empty;
}

leveldb::Status LeveldbValueStore::Delete(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(db_.get());

  leveldb::WriteBatch batch;
  batch.Delete(key);

  return db_->Write(leveldb::WriteOptions(), &batch);
}

bool LeveldbValueStore::DeleteDbFile() {
  db_.reset();  // release any lock on the directory
  if (!base::DeleteFile(db_path_, true /* recursive */)) {
    LOG(WARNING) << "Failed to delete LeveldbValueStore database at " <<
        db_path_.value();
    return false;
  }
  return true;
}

ValueStore::Status LeveldbValueStore::ToValueStoreError(
    const leveldb::Status& status) {
  CHECK(!status.IsNotFound());  // not an error

  std::string message = status.ToString();
  // The message may contain |db_path_|, which may be considered sensitive
  // data, and those strings are passed to the extension, so strip it out.
  base::ReplaceSubstringsAfterOffset(
      &message, 0u, db_path_.AsUTF8Unsafe(), "...");

  return Status(LevelDbToValueStoreStatus(status), message);
}
