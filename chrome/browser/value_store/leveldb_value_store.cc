// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/leveldb_value_store.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using content::BrowserThread;

namespace {

const char* kInvalidJson = "Invalid JSON";

ValueStore::ReadResult ReadFailure(const std::string& action,
                                   const std::string& reason) {
  CHECK_NE("", reason);
  return ValueStore::MakeReadResult(base::StringPrintf(
      "Failure to %s: %s", action.c_str(), reason.c_str()));
}

ValueStore::ReadResult ReadFailureForKey(const std::string& action,
                                         const std::string& key,
                                         const std::string& reason) {
  CHECK_NE("", reason);
  return ValueStore::MakeReadResult(base::StringPrintf(
      "Failure to %s for key %s: %s",
      action.c_str(), key.c_str(), reason.c_str()));
}

ValueStore::WriteResult WriteFailure(const std::string& action,
                                     const std::string& reason) {
  CHECK_NE("", reason);
  return ValueStore::MakeWriteResult(base::StringPrintf(
      "Failure to %s: %s", action.c_str(), reason.c_str()));
}

ValueStore::WriteResult WriteFailureForKey(const std::string& action,
                                           const std::string& key,
                                           const std::string& reason) {
  CHECK_NE("", reason);
  return ValueStore::MakeWriteResult(base::StringPrintf(
      "Failure to %s for key %s: %s",
      action.c_str(), key.c_str(), reason.c_str()));
}

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

LeveldbValueStore::LeveldbValueStore(const base::FilePath& db_path)
    : db_path_(db_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    LOG(WARNING) << error;
}

LeveldbValueStore::~LeveldbValueStore() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Delete the database from disk if it's empty (but only if we managed to
  // open it!). This is safe on destruction, assuming that we have exclusive
  // access to the database.
  if (db_ && IsEmpty()) {
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

ValueStore::ReadResult LeveldbValueStore::Get(const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeReadResult(error);

  scoped_ptr<Value> setting;
  error = ReadFromDb(leveldb::ReadOptions(), key, &setting);
  if (!error.empty())
    return ReadFailureForKey("get", key, error);

  DictionaryValue* settings = new DictionaryValue();
  if (setting.get())
    settings->SetWithoutPathExpansion(key, setting.release());
  return MakeReadResult(settings);
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeReadResult(error);

  leveldb::ReadOptions options;
  scoped_ptr<DictionaryValue> settings(new DictionaryValue());

  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> setting;
    error = ReadFromDb(options, *it, &setting);
    if (!error.empty())
      return ReadFailureForKey("get multiple items", *it, error);
    if (setting.get())
      settings->SetWithoutPathExpansion(*it, setting.release());
  }

  return MakeReadResult(settings.release());
}

ValueStore::ReadResult LeveldbValueStore::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeReadResult(error);

  base::JSONReader json_reader;
  leveldb::ReadOptions options = leveldb::ReadOptions();
  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  scoped_ptr<DictionaryValue> settings(new DictionaryValue());

  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    Value* value = json_reader.ReadToValue(it->value().ToString());
    if (value == NULL) {
      // TODO(kalman): clear the offending non-JSON value from the database.
      return ReadFailureForKey("get all", key, kInvalidJson);
    }
    settings->SetWithoutPathExpansion(key, value);
  }

  if (it->status().IsNotFound()) {
    NOTREACHED() << "IsNotFound() but iterating over all keys?!";
    return MakeReadResult(settings.release());
  }

  if (!it->status().ok())
    return ReadFailure("get all items", it->status().ToString());

  return MakeReadResult(settings.release());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeWriteResult(error);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  error = AddToBatch(options, key, value, &batch, changes.get());
  if (!error.empty())
    return WriteFailureForKey("find changes to set", key, error);

  error = WriteToDb(&batch);
  if (!error.empty())
    return WriteFailureForKey("set", key, error);
  return MakeWriteResult(changes.release());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeWriteResult(error);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (DictionaryValue::Iterator it(settings); it.HasNext(); it.Advance()) {
    error = AddToBatch(options, it.key(), it.value(), &batch, changes.get());
    if (!error.empty()) {
      return WriteFailureForKey("find changes to set multiple items",
                                it.key(),
                                error);
    }
  }

  error = WriteToDb(&batch);
  if (!error.empty())
    return WriteFailure("set multiple items", error);
  return MakeWriteResult(changes.release());
}

ValueStore::WriteResult LeveldbValueStore::Remove(const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeWriteResult(error);

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> old_value;
    error = ReadFromDb(leveldb::ReadOptions(), *it, &old_value);
    if (!error.empty()) {
      return WriteFailureForKey("find changes to remove multiple items",
                                *it,
                                error);
    }

    if (old_value.get()) {
      changes->push_back(ValueStoreChange(*it, old_value.release(), NULL));
      batch.Delete(*it);
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound())
    return WriteFailure("remove multiple items", status.ToString());
  return MakeWriteResult(changes.release());
}

ValueStore::WriteResult LeveldbValueStore::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error = EnsureDbIsOpen();
  if (!error.empty())
    return ValueStore::MakeWriteResult(error);

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
    if (!old_value) {
      // TODO: delete the bad JSON.
      return WriteFailureForKey("find changes to clear", key, kInvalidJson);
    }
    changes->push_back(ValueStoreChange(key, old_value, NULL));
    batch.Delete(key);
  }

  if (it->status().IsNotFound())
    NOTREACHED() << "IsNotFound() but clearing?!";
  else if (!it->status().ok())
    return WriteFailure("find changes to clear", it->status().ToString());

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (status.IsNotFound()) {
    NOTREACHED() << "IsNotFound() but clearing?!";
    return MakeWriteResult(changes.release());
  }
  if (!status.ok())
    return WriteFailure("clear", status.ToString());
  return MakeWriteResult(changes.release());
}

std::string LeveldbValueStore::EnsureDbIsOpen() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (db_.get())
    return "";

#if defined(OS_POSIX)
  std::string os_path(db_path_.value());
#elif defined(OS_WIN)
  std::string os_path = base::SysWideToUTF8(db_path_.value());
#endif

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, os_path, &db);
  if (!status.ok()) {
    // |os_path| may contain sensitive data, and these strings are passed
    // through to the extension, so strip that out.
    std::string status_string = status.ToString();
    ReplaceSubstringsAfterOffset(&status_string, 0u, os_path, "...");
    return base::StringPrintf("Failed to open database: %s",
                              status_string.c_str());
  }

  db_.reset(db);
  return "";
}

std::string LeveldbValueStore::ReadFromDb(
    leveldb::ReadOptions options,
    const std::string& key,
    scoped_ptr<Value>* setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(setting != NULL);
  std::string value_as_json;
  leveldb::Status s = db_->Get(options, key, &value_as_json);

  if (s.IsNotFound()) {
    // Despite there being no value, it was still a success.
    // Check this first because ok() is false on IsNotFound.
    return "";
  }

  if (!s.ok())
    return s.ToString();

  Value* value = base::JSONReader().ReadToValue(value_as_json);
  if (value == NULL) {
    // TODO(kalman): clear the offending non-JSON value from the database.
    return kInvalidJson;
  }

  setting->reset(value);
  return "";
}

std::string LeveldbValueStore::AddToBatch(
    ValueStore::WriteOptions options,
    const std::string& key,
    const base::Value& value,
    leveldb::WriteBatch* batch,
    ValueStoreChangeList* changes) {
  scoped_ptr<Value> old_value;
  if (!(options & NO_CHECK_OLD_VALUE)) {
    std::string error = ReadFromDb(leveldb::ReadOptions(), key, &old_value);
    if (!error.empty())
      return error;
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

  return "";
}

std::string LeveldbValueStore::WriteToDb(leveldb::WriteBatch* batch) {
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch);
  if (status.IsNotFound()) {
    NOTREACHED() << "IsNotFound() but writing?!";
    return "";
  }
  return status.ok() ? "" : status.ToString();
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
