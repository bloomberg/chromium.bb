// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/leveldb_value_store.h"

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

}  // namespace

LeveldbValueStore::LeveldbValueStore(const std::string& uma_client_name,
                                     const base::FilePath& db_path)
    : db_path_(db_path), open_histogram_(nullptr), restore_histogram_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

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

  Status open_error = EnsureDbIsOpen();
  if (!open_error.ok())
    return MakeReadResult(open_error);

  scoped_ptr<base::Value> setting;
  Status error = ReadFromDb(leveldb::ReadOptions(), key, &setting);
  if (!error.ok())
    return MakeReadResult(error);

  base::DictionaryValue* settings = new base::DictionaryValue();
  if (setting)
    settings->SetWithoutPathExpansion(key, setting.release());
  return MakeReadResult(make_scoped_ptr(settings));
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status open_error = EnsureDbIsOpen();
  if (!open_error.ok())
    return MakeReadResult(open_error);

  leveldb::ReadOptions options;
  scoped_ptr<base::DictionaryValue> settings(new base::DictionaryValue());

  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<base::Value> setting;
    Status error = ReadFromDb(options, *it, &setting);
    if (!error.ok())
      return MakeReadResult(error);
    if (setting)
      settings->SetWithoutPathExpansion(*it, setting.release());
  }

  return MakeReadResult(settings.Pass());
}

ValueStore::ReadResult LeveldbValueStore::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status open_error = EnsureDbIsOpen();
  if (!open_error.ok())
    return MakeReadResult(open_error);

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
    if (!value)
      return MakeReadResult(Status(CORRUPTION, kInvalidJson));
    settings->SetWithoutPathExpansion(key, value.Pass());
  }

  if (it->status().IsNotFound()) {
    NOTREACHED() << "IsNotFound() but iterating over all keys?!";
    return MakeReadResult(settings.Pass());
  }

  if (!it->status().ok())
    return MakeReadResult(ToValueStoreError(it->status()));

  return MakeReadResult(settings.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status open_error = EnsureDbIsOpen();
  if (!open_error.ok())
    return MakeWriteResult(open_error);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  Status batch_error = AddToBatch(options, key, value, &batch, changes.get());
  if (!batch_error.ok())
    return MakeWriteResult(batch_error);

  Status write_error = WriteToDb(&batch);
  return write_error.ok() ? MakeWriteResult(changes.Pass())
                          : MakeWriteResult(write_error);
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const base::DictionaryValue& settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status open_error = EnsureDbIsOpen();
  if (!open_error.ok())
    return MakeWriteResult(open_error);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (base::DictionaryValue::Iterator it(settings);
       !it.IsAtEnd(); it.Advance()) {
    Status batch_error =
        AddToBatch(options, it.key(), it.value(), &batch, changes.get());
    if (!batch_error.ok())
      return MakeWriteResult(batch_error);
  }

  Status write_error = WriteToDb(&batch);
  return write_error.ok() ? MakeWriteResult(changes.Pass())
                          : MakeWriteResult(write_error);
}

ValueStore::WriteResult LeveldbValueStore::Remove(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  Status open_error = EnsureDbIsOpen();
  if (!open_error.ok())
    return MakeWriteResult(open_error);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<base::Value> old_value;
    Status read_error = ReadFromDb(leveldb::ReadOptions(), *it, &old_value);
    if (!read_error.ok())
      return MakeWriteResult(read_error);

    if (old_value) {
      changes->push_back(ValueStoreChange(*it, old_value.release(), NULL));
      batch.Delete(*it);
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound())
    return MakeWriteResult(ToValueStoreError(status));
  return MakeWriteResult(changes.Pass());
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
    changes->push_back(
        ValueStoreChange(next_key, next_value.release(), NULL));
  }

  DeleteDbFile();
  return MakeWriteResult(changes.Pass());
}

bool LeveldbValueStore::Restore() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Possible to have a corrupted open database, so first close it.
  db_.reset();

  leveldb::Options options;
  options.create_if_missing = true;
  options.paranoid_checks = true;

  // Repair can drop an unbounded number of leveldb tables (key/value sets)
  leveldb::Status status = leveldb::RepairDB(db_path_.AsUTF8Unsafe(), options);
  if (status.ok()) {
    restore_histogram_->Add(LEVELDB_RESTORE_REPAIR_SUCCESS);
    return true;
  }

  if (DeleteDbFile()) {
    restore_histogram_->Add(LEVELDB_RESTORE_DELETE_SUCCESS);
    return true;
  }

  restore_histogram_->Add(LEVELDB_RESTORE_DELETE_FAILURE);
  return false;
}

bool LeveldbValueStore::RestoreKey(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  ReadResult result = Get(key);
  if (result->status().IsCorrupted()) {
    leveldb::WriteBatch batch;
    batch.Delete(key);
    ValueStore::Status error = WriteToDb(&batch);
    // If we can't delete the key, the restore failed.
    if (!error.ok())
      return false;
    result = Get(key);
  }

  // The restore succeeded if there is no corruption error.
  return !result->status().IsCorrupted();
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

ValueStore::Status LeveldbValueStore::EnsureDbIsOpen() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (db_)
    return Status();

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;

  leveldb::DB* db = NULL;
  leveldb::Status status =
      leveldb::DB::Open(options, db_path_.AsUTF8Unsafe(), &db);
  if (open_histogram_)
    open_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
  if (status.IsCorruption()) {
    // Returning a corruption error should result in Restore() being called.
    // However, since once a leveldb becomes corrupt it's unusable without
    // some kind of repair or delete, so do that right now.
    Restore();
  }
  if (!status.ok())
    return ToValueStoreError(status);

  CHECK(db);
  db_.reset(db);
  return Status();
}

ValueStore::Status LeveldbValueStore::ReadFromDb(
    leveldb::ReadOptions options,
    const std::string& key,
    scoped_ptr<base::Value>* setting) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(setting);

  std::string value_as_json;
  leveldb::Status s = db_->Get(options, key, &value_as_json);

  if (s.IsNotFound()) {
    // Despite there being no value, it was still a success. Check this first
    // because ok() is false on IsNotFound.
    return Status();
  }

  if (!s.ok())
    return ToValueStoreError(s);

  scoped_ptr<base::Value> value = base::JSONReader().ReadToValue(value_as_json);
  if (!value)
    return Status(CORRUPTION, kInvalidJson);

  *setting = value.Pass();
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
    Status read_error = ReadFromDb(leveldb::ReadOptions(), key, &old_value);
    if (read_error.IsCorrupted())
      batch->Delete(key);
    else if (!read_error.ok())
      return read_error;
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
  return status.ok() ? Status() : ToValueStoreError(status);
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
  CHECK(!status.ok());
  CHECK(!status.IsNotFound());  // not an error

  std::string message = status.ToString();
  // The message may contain |db_path_|, which may be considered sensitive
  // data, and those strings are passed to the extension, so strip it out.
  base::ReplaceSubstringsAfterOffset(
      &message, 0u, db_path_.AsUTF8Unsafe(), "...");

  return Status(CORRUPTION, message);
}
