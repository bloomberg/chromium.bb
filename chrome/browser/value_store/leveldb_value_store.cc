// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/leveldb_value_store.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/value_store/value_store_util.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace util = value_store_util;
using content::BrowserThread;

namespace {

const char kInvalidJson[] = "Invalid JSON";

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

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    LOG(WARNING) << open_error->message;
}

LeveldbValueStore::~LeveldbValueStore() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    return MakeReadResult(open_error.Pass());

  scoped_ptr<Value> setting;
  scoped_ptr<Error> error = ReadFromDb(leveldb::ReadOptions(), key, &setting);
  if (error)
    return MakeReadResult(error.Pass());

  DictionaryValue* settings = new DictionaryValue();
  if (setting)
    settings->SetWithoutPathExpansion(key, setting.release());
  return MakeReadResult(make_scoped_ptr(settings));
}

ValueStore::ReadResult LeveldbValueStore::Get(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    return MakeReadResult(open_error.Pass());

  leveldb::ReadOptions options;
  scoped_ptr<DictionaryValue> settings(new DictionaryValue());

  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> setting;
    scoped_ptr<Error> error = ReadFromDb(options, *it, &setting);
    if (error)
      return MakeReadResult(error.Pass());
    if (setting)
      settings->SetWithoutPathExpansion(*it, setting.release());
  }

  return MakeReadResult(settings.Pass());
}

ValueStore::ReadResult LeveldbValueStore::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    return MakeReadResult(open_error.Pass());

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
    if (!value) {
      return MakeReadResult(
          Error::Create(CORRUPTION, kInvalidJson, util::NewKey(key)));
    }
    settings->SetWithoutPathExpansion(key, value);
  }

  if (it->status().IsNotFound()) {
    NOTREACHED() << "IsNotFound() but iterating over all keys?!";
    return MakeReadResult(settings.Pass());
  }

  if (!it->status().ok())
    return MakeReadResult(ToValueStoreError(it->status(), util::NoKey()));

  return MakeReadResult(settings.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    return MakeWriteResult(open_error.Pass());

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  scoped_ptr<Error> batch_error =
      AddToBatch(options, key, value, &batch, changes.get());
  if (batch_error)
    return MakeWriteResult(batch_error.Pass());

  scoped_ptr<Error> write_error = WriteToDb(&batch);
  return write_error ? MakeWriteResult(write_error.Pass())
                     : MakeWriteResult(changes.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Set(
    WriteOptions options, const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    return MakeWriteResult(open_error.Pass());

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (DictionaryValue::Iterator it(settings); !it.IsAtEnd(); it.Advance()) {
    scoped_ptr<Error> batch_error =
        AddToBatch(options, it.key(), it.value(), &batch, changes.get());
    if (batch_error)
      return MakeWriteResult(batch_error.Pass());
  }

  scoped_ptr<Error> write_error = WriteToDb(&batch);
  return write_error ? MakeWriteResult(write_error.Pass())
                     : MakeWriteResult(changes.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Remove(const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult LeveldbValueStore::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<Error> open_error = EnsureDbIsOpen();
  if (open_error)
    return MakeWriteResult(open_error.Pass());

  leveldb::WriteBatch batch;
  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> old_value;
    scoped_ptr<Error> read_error =
        ReadFromDb(leveldb::ReadOptions(), *it, &old_value);
    if (read_error)
      return MakeWriteResult(read_error.Pass());

    if (old_value) {
      changes->push_back(ValueStoreChange(*it, old_value.release(), NULL));
      batch.Delete(*it);
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound())
    return MakeWriteResult(ToValueStoreError(status, util::NoKey()));
  return MakeWriteResult(changes.Pass());
}

ValueStore::WriteResult LeveldbValueStore::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());

  ReadResult read_result = Get();
  if (read_result->HasError())
    return MakeWriteResult(read_result->PassError());

  base::DictionaryValue& whole_db = read_result->settings();
  while (!whole_db.empty()) {
    std::string next_key = DictionaryValue::Iterator(whole_db).key();
    scoped_ptr<base::Value> next_value;
    whole_db.RemoveWithoutPathExpansion(next_key, &next_value);
    changes->push_back(
        ValueStoreChange(next_key, next_value.release(), NULL));
  }

  DeleteDbFile();
  return MakeWriteResult(changes.Pass());
}

scoped_ptr<ValueStore::Error> LeveldbValueStore::EnsureDbIsOpen() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (db_)
    return util::NoError();

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;

  leveldb::DB* db = NULL;
  leveldb::Status status =
      leveldb::DB::Open(options, db_path_.AsUTF8Unsafe(), &db);
  if (!status.ok())
    return ToValueStoreError(status, util::NoKey());

  CHECK(db);
  db_.reset(db);
  return util::NoError();
}

scoped_ptr<ValueStore::Error> LeveldbValueStore::ReadFromDb(
    leveldb::ReadOptions options,
    const std::string& key,
    scoped_ptr<Value>* setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(setting);

  std::string value_as_json;
  leveldb::Status s = db_->Get(options, key, &value_as_json);

  if (s.IsNotFound()) {
    // Despite there being no value, it was still a success. Check this first
    // because ok() is false on IsNotFound.
    return util::NoError();
  }

  if (!s.ok())
    return ToValueStoreError(s, util::NewKey(key));

  Value* value = base::JSONReader().ReadToValue(value_as_json);
  if (!value)
    return Error::Create(CORRUPTION, kInvalidJson, util::NewKey(key));

  setting->reset(value);
  return util::NoError();
}

scoped_ptr<ValueStore::Error> LeveldbValueStore::AddToBatch(
    ValueStore::WriteOptions options,
    const std::string& key,
    const base::Value& value,
    leveldb::WriteBatch* batch,
    ValueStoreChangeList* changes) {
  bool write_new_value = true;

  if (!(options & NO_GENERATE_CHANGES)) {
    scoped_ptr<Value> old_value;
    scoped_ptr<Error> read_error =
        ReadFromDb(leveldb::ReadOptions(), key, &old_value);
    if (read_error)
      return read_error.Pass();
    if (!old_value || !old_value->Equals(&value)) {
      changes->push_back(
          ValueStoreChange(key, old_value.release(), value.DeepCopy()));
    } else {
      write_new_value = false;
    }
  }

  if (write_new_value) {
    std::string value_as_json;
    base::JSONWriter::Write(&value, &value_as_json);
    batch->Put(key, value_as_json);
  }

  return util::NoError();
}

scoped_ptr<ValueStore::Error> LeveldbValueStore::WriteToDb(
    leveldb::WriteBatch* batch) {
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch);
  return status.ok() ? util::NoError()
                     : ToValueStoreError(status, util::NoKey());
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

void LeveldbValueStore::DeleteDbFile() {
  db_.reset();  // release any lock on the directory
  if (!base::DeleteFile(db_path_, true /* recursive */)) {
    LOG(WARNING) << "Failed to delete LeveldbValueStore database at " <<
        db_path_.value();
  }
}

scoped_ptr<ValueStore::Error> LeveldbValueStore::ToValueStoreError(
    const leveldb::Status& status,
    scoped_ptr<std::string> key) {
  CHECK(!status.ok());
  CHECK(!status.IsNotFound());  // not an error

  std::string message = status.ToString();
  // The message may contain |db_path_|, which may be considered sensitive
  // data, and those strings are passed to the extension, so strip it out.
  ReplaceSubstringsAfterOffset(&message, 0u, db_path_.AsUTF8Unsafe(), "...");

  return Error::Create(CORRUPTION, message, key.Pass());
}
