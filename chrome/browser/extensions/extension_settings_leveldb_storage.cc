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

// General closure type for computing a value on the FILE thread and calling
// back on the UI thread.  The *OnFileThread methods should run on the file
// thread, all others should run on the UI thread.
//
// Subclasses should implement RunOnFileThread(), manipulating settings_
// or error_, then call either SucceedOnFileThread() or FailOnFileThread().
class ResultClosure {
 public:
  // Ownership of callback and settings are taken.
  // Settings may be NULL (for Remove/Clear).
  ResultClosure(
      leveldb::DB* db,
      DictionaryValue* settings,
      ExtensionSettingsStorage::Callback* callback)
      : db_(db), settings_(settings), callback_(callback) {}

  virtual ~ResultClosure() {}

  void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&ResultClosure::RunOnFileThread, base::Unretained(this)));
  }

 protected:
  virtual void RunOnFileThread() = 0;

  void SucceedOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ResultClosure::Succeed, base::Unretained(this)));
  }

  void FailOnFileThread(std::string error) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    error_ = error;
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ResultClosure::Fail, base::Unretained(this)));
  }

  // Helper for the Get() methods; reads a single key from the database using
  // an existing leveldb options object.
  // Returns whether the read was successful, in which case the given settings
  // will have the value set.  Otherwise, error will be set.
  bool ReadFromDb(
      leveldb::ReadOptions options,
      const std::string& key,
      DictionaryValue* settings,
      std::string* error) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    std::string value_as_json;
    leveldb::Status s = db_->Get(options, key, &value_as_json);
    if (s.ok()) {
      base::JSONReader json_reader;
      Value* value = json_reader.JsonToValue(value_as_json, false, false);
      if (value != NULL) {
        settings->SetWithoutPathExpansion(key, value);
        return true;
      } else {
        // TODO(kalman): clear the offending non-JSON value from the database.
        LOG(ERROR) << "Invalid JSON: " << value_as_json;
        *error = "Invalid JSON";
        return false;
      }
    } else if (s.IsNotFound()) {
      // Despite there being no value, it was still a success.
      return true;
    } else {
      LOG(ERROR) << "Error reading from database: " << s.ToString();
      *error = s.ToString();
      return false;
    }
  }

  // The leveldb database this closure interacts with.  Owned by an
  // ExtensionSettingsLeveldbStorage instance, but only accessed from the FILE
  // thread part of the closure (RunOnFileThread), and only deleted on the FILE
  // thread from DeleteSoon() method, guaranteed to run afterwards (see
  // marked_for_deletion_).
  leveldb::DB* db_;

  scoped_ptr<DictionaryValue> settings_;
  std::string error_;

 private:
  void Succeed() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    callback_->OnSuccess(settings_.release());
    delete this;
  }

  void Fail() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    callback_->OnFailure(error_);
    delete this;
  }

  scoped_ptr<ExtensionSettingsStorage::Callback> callback_;

  DISALLOW_COPY_AND_ASSIGN(ResultClosure);
};

// Result closure for Get(key).
class Get1ResultClosure : public ResultClosure {
 public:
  Get1ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback,
      const std::string& key)
      : ResultClosure(db, new DictionaryValue(), callback), key_(key) {}

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    if (ReadFromDb(leveldb::ReadOptions(), key_, settings_.get(), &error_)) {
      SucceedOnFileThread();
    } else {
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }

 private:
  std::string key_;
};

// Result closure for Get(keys).
class Get2ResultClosure : public ResultClosure {
 public:
  // Ownership of keys is taken.
  Get2ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback,
      ListValue* keys)
      : ResultClosure(db, new DictionaryValue(), callback), keys_(keys) {}

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    leveldb::ReadOptions options;
    // All interaction with the db is done on the same thread, so snapshotting
    // isn't strictly necessary.  This is just defensive.
    options.snapshot = db_->GetSnapshot();
    bool success = true;
    std::string key;

    for (ListValue::const_iterator it = keys_->begin();
        success && it != keys_->end(); ++it) {
      if ((*it)->GetAsString(&key)) {
        if (!ReadFromDb(options, key, settings_.get(), &error_)) {
          success = false;
        }
      }
    }
    db_->ReleaseSnapshot(options.snapshot);

    if (success) {
      SucceedOnFileThread();
    } else {
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }

 private:
  scoped_ptr<ListValue> keys_;
};

// Result closure for Get() .
class Get3ResultClosure : public ResultClosure {
 public:
  Get3ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback)
      : ResultClosure(db, new DictionaryValue(), callback) {
  }

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    base::JSONReader json_reader;
    leveldb::ReadOptions options = leveldb::ReadOptions();
    // All interaction with the db is done on the same thread, so snapshotting
    // isn't strictly necessary.  This is just defensive.
    options.snapshot = db_->GetSnapshot();
    scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      Value* value =
          json_reader.JsonToValue(it->value().ToString(), false, false);
      if (value != NULL) {
        settings_->SetWithoutPathExpansion(it->key().ToString(), value);
      } else {
        // TODO(kalman): clear the offending non-JSON value from the database.
        LOG(ERROR) << "Invalid JSON: " << it->value().ToString();
      }
    }
    db_->ReleaseSnapshot(options.snapshot);

    if (!it->status().ok()) {
      LOG(ERROR) << "DB iteration failed: " << it->status().ToString();
      FailOnFileThread(kGenericOnFailureMessage);
    } else {
      SucceedOnFileThread();
    }
  }
};

// Result closure for Set(key, value).
class Set1ResultClosure : public ResultClosure {
 public:
  // Ownership of the value is taken.
  Set1ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback,
      const std::string& key,
      Value* value)
      : ResultClosure(db, new DictionaryValue(), callback),
        key_(key),
        value_(value) {}

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    std::string value_as_json;
    base::JSONWriter::Write(value_.get(), false, &value_as_json);
    leveldb::Status status =
        db_->Put(leveldb::WriteOptions(), key_, value_as_json);
    if (status.ok()) {
      settings_->SetWithoutPathExpansion(key_, value_.release());
      SucceedOnFileThread();
    } else {
      LOG(WARNING) << "DB write failed: " << status.ToString();
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }

 private:
  std::string key_;
  scoped_ptr<Value> value_;
};

// Result callback for Set(values).
class Set2ResultClosure : public ResultClosure {
 public:
  // Ownership of values is taken.
  Set2ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback,
      DictionaryValue* values)
      : ResultClosure(db, new DictionaryValue(), callback), values_(values) {
  }

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // Gather keys since a dictionary can't be modified during iteration.
    std::vector<std::string> keys;
    for (DictionaryValue::key_iterator it = values_->begin_keys();
        it != values_->end_keys(); ++it) {
      keys.push_back(*it);
    }
    // Write values while transferring ownership from values_ to settings_.
    std::string value_as_json;
    leveldb::WriteBatch batch;
    for (unsigned i = 0; i < keys.size(); ++i) {
      Value* value = NULL;
      values_->RemoveWithoutPathExpansion(keys[i], &value);
      base::JSONWriter::Write(value, false, &value_as_json);
      batch.Put(keys[i], value_as_json);
      settings_->SetWithoutPathExpansion(keys[i], value);
    }
    DCHECK(values_->empty());

    leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
    if (status.ok()) {
      SucceedOnFileThread();
    } else {
      LOG(WARNING) << "DB batch write failed: " << status.ToString();
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }

 private:
  scoped_ptr<DictionaryValue> values_; // will be empty on destruction
};

// Result closure for Remove(key).
class Remove1ResultClosure : public ResultClosure {
 public:
  Remove1ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback,
      const std::string& key)
      : ResultClosure(db, NULL, callback), key_(key) {
  }

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    leveldb::Status status = db_->Delete(leveldb::WriteOptions(), key_);
    if (status.ok() || status.IsNotFound()) {
      SucceedOnFileThread();
    } else {
      LOG(WARNING) << "DB delete failed: " << status.ToString();
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }

 private:
  std::string key_;
};

// Result closure for Remove(keys).
class Remove2ResultClosure : public ResultClosure {
 public:
  // Ownership of keys is taken.
  Remove2ResultClosure(
      leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback,
      ListValue* keys)
      : ResultClosure(db, NULL, callback), keys_(keys) {
  }

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    std::string key;
    leveldb::WriteBatch batch;
    for (ListValue::const_iterator it = keys_->begin();
        it != keys_->end(); ++it) {
      if ((*it)->GetAsString(&key)) {
        batch.Delete(key);
      }
    }

    leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
    if (status.ok() || status.IsNotFound()) {
      SucceedOnFileThread();
    } else {
      LOG(WARNING) << "DB batch delete failed: " << status.ToString();
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }

 private:
  scoped_ptr<ListValue> keys_;
};

// Result closure for Clear().
class ClearResultClosure : public ResultClosure {
 public:
  ClearResultClosure(leveldb::DB* db,
      ExtensionSettingsStorage::Callback* callback)
      : ResultClosure(db, NULL, callback) {
  }

 protected:
  virtual void RunOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    leveldb::ReadOptions options;
    // All interaction with the db is done on the same thread, so snapshotting
    // isn't strictly necessary.  This is just defensive.
    options.snapshot = db_->GetSnapshot();
    scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
    leveldb::WriteBatch batch;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      batch.Delete(it->key());
    }
    db_->ReleaseSnapshot(options.snapshot);

    if (it->status().ok()) {
      leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
      if (status.ok() || status.IsNotFound()) {
        SucceedOnFileThread();
      } else {
        LOG(WARNING) << "Clear failed: " << status.ToString();
        FailOnFileThread(kGenericOnFailureMessage);
      }
    } else {
      LOG(WARNING) << "Clear iteration failed: " << it->status().ToString();
      FailOnFileThread(kGenericOnFailureMessage);
    }
  }
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
    leveldb::DB* db)
    : db_(db), marked_for_deletion_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsLeveldbStorage::~ExtensionSettingsLeveldbStorage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void ExtensionSettingsLeveldbStorage::DeleteSoon() {
  CHECK(!marked_for_deletion_);
  marked_for_deletion_ = true;
  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, this);
}

void ExtensionSettingsLeveldbStorage::Get(
    const std::string& key, ExtensionSettingsStorage::Callback* callback) {
  CHECK(!marked_for_deletion_);
  (new Get1ResultClosure(db_.get(), callback, key))->Run();
}

void ExtensionSettingsLeveldbStorage::Get(
    const ListValue& keys, ExtensionSettingsStorage::Callback* callback) {
  CHECK(!marked_for_deletion_);
  (new Get2ResultClosure(db_.get(), callback, keys.DeepCopy()))->Run();
}

void ExtensionSettingsLeveldbStorage::Get(
    ExtensionSettingsStorage::Callback* callback) {
  CHECK(!marked_for_deletion_);
  (new Get3ResultClosure(db_.get(), callback))->Run();
}

void ExtensionSettingsLeveldbStorage::Set(
    const std::string& key,
    const Value& value,
    ExtensionSettingsStorage::Callback* callback) {
  CHECK(!marked_for_deletion_);
  (new Set1ResultClosure(db_.get(), callback, key, value.DeepCopy()))->Run();
}

void ExtensionSettingsLeveldbStorage::Set(
    const DictionaryValue& values,
    ExtensionSettingsStorage::Callback* callback) {
  CHECK(!marked_for_deletion_);
  (new Set2ResultClosure(db_.get(), callback, values.DeepCopy()))->Run();
}

void ExtensionSettingsLeveldbStorage::Remove(
    const std::string& key, ExtensionSettingsStorage::Callback *callback) {
  CHECK(!marked_for_deletion_);
  (new Remove1ResultClosure(db_.get(), callback, key))->Run();
}

void ExtensionSettingsLeveldbStorage::Remove(
    const ListValue& keys, ExtensionSettingsStorage::Callback *callback) {
  CHECK(!marked_for_deletion_);
  (new Remove2ResultClosure(db_.get(), callback, keys.DeepCopy()))->Run();
}

void ExtensionSettingsLeveldbStorage::Clear(
    ExtensionSettingsStorage::Callback* callback) {
  CHECK(!marked_for_deletion_);
  (new ClearResultClosure(db_.get(), callback))->Run();
}
