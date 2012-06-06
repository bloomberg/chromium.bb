// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/leveldb_value_store.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using content::BrowserThread;

namespace {

// Generic error message on failure.
const char* kGenericOnFailureMessage = "Failure accessing database";

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

// static
LeveldbValueStore* LeveldbValueStore::Create(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

#if defined(OS_POSIX)
  std::string os_path(path.value());
#elif defined(OS_WIN)
  std::string os_path = base::SysWideToUTF8(path.value());
#endif

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, os_path, &db);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to create leveldb at " << path.value() <<
        ": " << status.ToString();
    return NULL;
  }
  return new LeveldbValueStore(path, db);
}

LeveldbValueStore::LeveldbValueStore(
    const FilePath& db_path, leveldb::DB* db)
    : db_path_(db_path), db_(db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

LeveldbValueStore::~LeveldbValueStore() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Delete the database from disk if it's empty.  This is safe on destruction,
  // assuming that we have exclusive access to the database.
  if (IsEmpty()) {
    // Close |db_| now to release any lock on the directory.
    db_.reset();
    if (!file_util::Delete(db_path_, true)) {
      LOG(WARNING) << "Failed to delete LeveldbValueStore database " <<
          db_path_.value();
    }
  }
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

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<Value> setting;
  if (!ReadFromDb(leveldb::ReadOptions(), key, &setting)) {
    return MakeReadResult(kGenericOnFailureMessage);
  }
  DictionaryValue* settings = new DictionaryValue();
  if (setting.get()) {
    settings->SetWithoutPathExpansion(key, setting.release());
  }
  return MakeReadResult(settings);
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  leveldb::ReadOptions options;
  scoped_ptr<DictionaryValue> settings(new DictionaryValue());

  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> setting;
    if (!ReadFromDb(options, *it, &setting)) {
      return MakeReadResult(kGenericOnFailureMessage);
    }
    if (setting.get()) {
      settings->SetWithoutPathExpansion(*it, setting.release());
    }
  }

  return MakeReadResult(settings.release());
}

ValueStore::ReadResult LeveldbValueStore::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::JSONReader json_reader;
  leveldb::ReadOptions options = leveldb::ReadOptions();
  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  scoped_ptr<DictionaryValue> settings(new DictionaryValue());

  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    Value* value = json_reader.ReadToValue(it->value().ToString());
    if (value != NULL) {
      settings->SetWithoutPathExpansion(it->key().ToString(), value);
    } else {
      // TODO(kalman): clear the offending non-JSON value from the database.
      LOG(ERROR) << "Invalid JSON: " << it->value().ToString();
    }
  }

  if (!it->status().ok()) {
    LOG(ERROR) << "DB iteration failed: " << it->status().ToString();
    return MakeReadResult(kGenericOnFailureMessage);
  }

  return MakeReadResult(settings.release());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  AddToBatch(options, key, value, &batch, changes.get());
  return WriteToDb(&batch, changes.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (DictionaryValue::Iterator it(settings); it.HasNext(); it.Advance()) {
    if (!AddToBatch(options, it.key(), it.value(), &batch, changes.get()))
      return MakeWriteResult(kGenericOnFailureMessage);
  }

  return WriteToDb(&batch, changes.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<std::string> keys;
  keys.push_back(key);
  return Remove(keys);
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> old_value;
    if (!ReadFromDb(leveldb::ReadOptions(), *it, &old_value)) {
      return MakeWriteResult(kGenericOnFailureMessage);
    }

    if (old_value.get()) {
      changes->push_back(
          ValueStoreChange(*it, old_value.release(), NULL));
      batch.Delete(*it);
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound()) {
    LOG(WARNING) << "DB batch delete failed: " << status.ToString();
    return MakeWriteResult(kGenericOnFailureMessage);
  }

  return MakeWriteResult(changes.release());
}

ValueStore::WriteResult LeveldbValueStore::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  leveldb::ReadOptions read_options;
  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  ScopedSnapshot snapshot(db_.get());
  read_options.snapshot = snapshot.get();
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(read_options));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    const std::string key = it->key().ToString();
    const std::string old_value_json = it->value().ToString();
    Value* old_value = base::JSONReader().ReadToValue(old_value_json);
    if (old_value) {
      changes->push_back(ValueStoreChange(key, old_value, NULL));
    } else {
      LOG(ERROR) << "Invalid JSON in database: " << old_value_json;
    }
    batch.Delete(key);
  }

  if (!it->status().ok()) {
    LOG(WARNING) << "Clear iteration failed: " << it->status().ToString();
    return MakeWriteResult(kGenericOnFailureMessage);
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound()) {
    LOG(WARNING) << "Clear failed: " << status.ToString();
    return MakeWriteResult(kGenericOnFailureMessage);
  }

  return MakeWriteResult(changes.release());
}

bool LeveldbValueStore::ReadFromDb(
    leveldb::ReadOptions options,
    const std::string& key,
    scoped_ptr<Value>* setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(setting != NULL);
  std::string value_as_json;
  leveldb::Status s = db_->Get(options, key, &value_as_json);

  if (s.IsNotFound()) {
    // Despite there being no value, it was still a success.
    return true;
  }

  if (!s.ok()) {
    LOG(ERROR) << "Error reading from database: " << s.ToString();
    return false;
  }

  Value* value = base::JSONReader().ReadToValue(value_as_json);
  if (value == NULL) {
    // TODO(kalman): clear the offending non-JSON value from the database.
    LOG(ERROR) << "Invalid JSON in database: " << value_as_json;
    return false;
  }

  setting->reset(value);
  return true;
}

bool LeveldbValueStore::AddToBatch(
    ValueStore::WriteOptions options,
    const std::string& key,
    const base::Value& value,
    leveldb::WriteBatch* batch,
    ValueStoreChangeList* changes) {
  scoped_ptr<Value> old_value;
  if (!(options & NO_CHECK_OLD_VALUE)) {
    if (!ReadFromDb(leveldb::ReadOptions(), key, &old_value))
      return false;
  }

  if (!old_value.get() || !old_value->Equals(&value)) {
    if (!(options & NO_GENERATE_CHANGES)) {
      changes->push_back(
          ValueStoreChange(key, old_value.release(), value.DeepCopy()));
    }
    std::string value_as_json;
    base::JSONWriter::Write(&value, &value_as_json);
    batch->Put(key, value_as_json);
  }

  return true;
}

ValueStore::WriteResult LeveldbValueStore::WriteToDb(
    leveldb::WriteBatch* batch,
    scoped_ptr<ValueStoreChangeList> changes) {
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch);
  if (!status.ok()) {
    LOG(WARNING) << "DB batch write failed: " << status.ToString();
    return MakeWriteResult(kGenericOnFailureMessage);
  }

  return MakeWriteResult(changes.release());
}

bool LeveldbValueStore::IsEmpty() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));

  it->SeekToFirst();
  bool is_empty = !it->Valid();
  if (!it->status().ok()) {
    LOG(ERROR) << "Checking DB emptiness failed: " << it->status().ToString();
    return false;
  }
  return is_empty;
}
