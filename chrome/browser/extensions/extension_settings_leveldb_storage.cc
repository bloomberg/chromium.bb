// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_leveldb_storage.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "content/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

// Generic error message sent to extensions on failure.
const char* kGenericOnFailureMessage = "Extension settings failed";

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

/* static */
ExtensionSettingsLeveldbStorage* ExtensionSettingsLeveldbStorage::Create(
      const FilePath& base_path,
      const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath path = base_path.AppendASCII(extension_id);

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
  return new ExtensionSettingsLeveldbStorage(db);
}

ExtensionSettingsLeveldbStorage::ExtensionSettingsLeveldbStorage(
    leveldb::DB* db) : db_(db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsLeveldbStorage::~ExtensionSettingsLeveldbStorage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Get(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<Value> setting;
  if (!ReadFromDb(leveldb::ReadOptions(), key, &setting)) {
    return Result(kGenericOnFailureMessage);
  }
  DictionaryValue* settings = new DictionaryValue();
  if (setting.get()) {
    settings->SetWithoutPathExpansion(key, setting.release());
  }
  return Result(settings, NULL);
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Get(
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
      return Result(kGenericOnFailureMessage);
    }
    if (setting.get()) {
      settings->SetWithoutPathExpansion(*it, setting.release());
    }
  }

  return Result(settings.release(), NULL);
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Get() {
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
    Value* value =
        json_reader.JsonToValue(it->value().ToString(), false, false);
    if (value != NULL) {
      settings->SetWithoutPathExpansion(it->key().ToString(), value);
    } else {
      // TODO(kalman): clear the offending non-JSON value from the database.
      LOG(ERROR) << "Invalid JSON: " << it->value().ToString();
    }
  }

  if (!it->status().ok()) {
    LOG(ERROR) << "DB iteration failed: " << it->status().ToString();
    return Result(kGenericOnFailureMessage);
  }

  return Result(settings.release(), NULL);
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Set(
    const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(settings);
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Set(
    const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<std::set<std::string> > changed_keys(new std::set<std::string>());
  std::string value_as_json;
  leveldb::WriteBatch batch;

  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    scoped_ptr<Value> original_value;
    if (!ReadFromDb(leveldb::ReadOptions(), *it, &original_value)) {
      return Result(kGenericOnFailureMessage);
    }

    Value* new_value = NULL;
    settings.GetWithoutPathExpansion(*it, &new_value);
    if (!original_value.get() || !original_value->Equals(new_value)) {
      changed_keys->insert(*it);
      base::JSONWriter::Write(new_value, false, &value_as_json);
      batch.Put(*it, value_as_json);
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    LOG(WARNING) << "DB batch write failed: " << status.ToString();
    return Result(kGenericOnFailureMessage);
  }

  return Result(settings.DeepCopy(), changed_keys.release());
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Remove(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<std::string> keys;
  keys.push_back(key);
  return Remove(keys);
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<std::set<std::string> > changed_keys(new std::set<std::string>());

  leveldb::WriteBatch batch;
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<Value> original_value;
    if (!ReadFromDb(leveldb::ReadOptions(), *it, &original_value)) {
      return Result(kGenericOnFailureMessage);
    }

    if (original_value.get()) {
      changed_keys->insert(*it);
      batch.Delete(*it);
    }
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound()) {
    LOG(WARNING) << "DB batch delete failed: " << status.ToString();
    return Result(kGenericOnFailureMessage);
  }

  return Result(NULL, changed_keys.release());
}

ExtensionSettingsStorage::Result ExtensionSettingsLeveldbStorage::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<std::set<std::string> > changed_keys(new std::set<std::string>());
  leveldb::ReadOptions options;
  // All interaction with the db is done on the same thread, so snapshotting
  // isn't strictly necessary.  This is just defensive.
  leveldb::WriteBatch batch;

  ScopedSnapshot snapshot(db_.get());
  options.snapshot = snapshot.get();
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    changed_keys->insert(it->key().ToString());
    batch.Delete(it->key());
  }

  if (!it->status().ok()) {
    LOG(WARNING) << "Clear iteration failed: " << it->status().ToString();
    return Result(kGenericOnFailureMessage);
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok() && !status.IsNotFound()) {
    LOG(WARNING) << "Clear failed: " << status.ToString();
    return Result(kGenericOnFailureMessage);
  }

  return Result(NULL, changed_keys.release());
}

bool ExtensionSettingsLeveldbStorage::ReadFromDb(
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

  Value* value = base::JSONReader().JsonToValue(value_as_json, false, false);
  if (value == NULL) {
    // TODO(kalman): clear the offending non-JSON value from the database.
    LOG(ERROR) << "Invalid JSON in database: " << value_as_json;
    return false;
  }

  setting->reset(value);
  return true;
}
