// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/leveldb_pref_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

enum ErrorMasks {
  OPENED = 1 << 0,
  DESTROYED = 1 << 1,
  REPAIRED = 1 << 2,
  DESTROY_FAILED = 1 << 3,
  REPAIR_FAILED = 1 << 4,
  IO_ERROR = 1 << 5,
  DATA_LOST = 1 << 6,
  ITER_NOT_OK = 1 << 7,
  FILE_NOT_SPECIFIED = 1 << 8,
};

PersistentPrefStore::PrefReadError IntToPrefReadError(int error) {
  DCHECK(error);
  if (error == FILE_NOT_SPECIFIED)
    return PersistentPrefStore::PREF_READ_ERROR_FILE_NOT_SPECIFIED;
  if (error == OPENED)
    return PersistentPrefStore::PREF_READ_ERROR_NONE;
  if (error & IO_ERROR)
    return PersistentPrefStore::PREF_READ_ERROR_LEVELDB_IO;
  if (error & OPENED)
    return PersistentPrefStore::PREF_READ_ERROR_LEVELDB_CORRUPTION;
  return PersistentPrefStore::PREF_READ_ERROR_LEVELDB_CORRUPTION_READ_ONLY;
}

} // namespace

struct LevelDBPrefStore::ReadingResults {
  ReadingResults() : no_dir(true), error(0) {}
  bool no_dir;
  scoped_ptr<leveldb::DB> db;
  scoped_ptr<PrefValueMap> value_map;
  int error;
};

// An instance of this class is created on the UI thread but is used
// exclusively on the FILE thread.
class LevelDBPrefStore::FileThreadSerializer {
 public:
  explicit FileThreadSerializer(scoped_ptr<leveldb::DB> db) : db_(db.Pass()) {}
  void WriteToDatabase(
      std::map<std::string, std::string>* keys_to_set,
      std::set<std::string>* keys_to_delete) {
    DCHECK(keys_to_set->size() > 0 || keys_to_delete->size() > 0);
    leveldb::WriteBatch batch;
    for (std::map<std::string, std::string>::iterator iter =
             keys_to_set->begin();
         iter != keys_to_set->end();
         iter++) {
      batch.Put(iter->first, iter->second);
    }

    for (std::set<std::string>::iterator iter = keys_to_delete->begin();
         iter != keys_to_delete->end();
         iter++) {
      batch.Delete(*iter);
    }

    leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);

    // DCHECK is fine; the corresponding error is ignored in JsonPrefStore.
    // There's also no API available to surface the error back up to the caller.
    // TODO(dgrogan): UMA?
    DCHECK(status.ok()) << status.ToString();
  }

 private:
  scoped_ptr<leveldb::DB> db_;
  DISALLOW_COPY_AND_ASSIGN(FileThreadSerializer);
};

bool MoveDirectoryAside(const base::FilePath& path) {
  base::FilePath bad_path = path.AppendASCII(".bad");
  if (!base::Move(path, bad_path)) {
    base::DeleteFile(bad_path, true);
    return false;
  }
  return true;
}

/* static */
void LevelDBPrefStore::OpenDB(const base::FilePath& path,
                              ReadingResults* reading_results) {
  DCHECK_EQ(0, reading_results->error);
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  while (1) {
    leveldb::Status status =
        leveldb::DB::Open(options, path.AsUTF8Unsafe(), &db);
    if (status.ok()) {
      reading_results->db.reset(db);
      reading_results->error |= OPENED;
      break;
    }
    if (leveldb_env::IsIOError(status)) {
      reading_results->error |= IO_ERROR;
      break;
    }
    if (reading_results->error & DESTROYED)
      break;

    DCHECK(!(reading_results->error & REPAIR_FAILED));
    if (!(reading_results->error & REPAIRED)) {
      status = leveldb::RepairDB(path.AsUTF8Unsafe(), options);
      if (status.ok()) {
        reading_results->error |= REPAIRED;
        continue;
      }
      reading_results->error |= REPAIR_FAILED;
    }
    if (!MoveDirectoryAside(path)) {
      status = leveldb::DestroyDB(path.AsUTF8Unsafe(), options);
      if (!status.ok()) {
        reading_results->error |= DESTROY_FAILED;
        break;
      }
    }
    reading_results->error |= DESTROYED;
  }
  DCHECK(reading_results->error);
  DCHECK(!((reading_results->error & OPENED) &&
           (reading_results->error & DESTROY_FAILED)));
  DCHECK(reading_results->error != (REPAIR_FAILED | OPENED));
}

/* static */
scoped_ptr<LevelDBPrefStore::ReadingResults> LevelDBPrefStore::DoReading(
    const base::FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<ReadingResults> reading_results(new ReadingResults);

  reading_results->no_dir = !base::PathExists(path.DirName());
  OpenDB(path, reading_results.get());
  if (!reading_results->db) {
    DCHECK(!(reading_results->error & OPENED));
    return reading_results.Pass();
  }

  DCHECK(reading_results->error & OPENED);
  reading_results->value_map.reset(new PrefValueMap);
  scoped_ptr<leveldb::Iterator> it(
      reading_results->db->NewIterator(leveldb::ReadOptions()));
  // TODO(dgrogan): Is it really necessary to check it->status() each iteration?
  for (it->SeekToFirst(); it->Valid() && it->status().ok(); it->Next()) {
    const std::string value_string = it->value().ToString();
    JSONStringValueSerializer deserializer(value_string);
    std::string error_message;
    int error_code;
    base::Value* json_value =
        deserializer.Deserialize(&error_code, &error_message);
    if (json_value) {
      reading_results->value_map->SetValue(it->key().ToString(), json_value);
    } else {
      DLOG(ERROR) << "Invalid json for key " << it->key().ToString()
                  << ": " << error_message;
      reading_results->error |= DATA_LOST;
    }
  }

  if (!it->status().ok())
    reading_results->error |= ITER_NOT_OK;

  return reading_results.Pass();
}

LevelDBPrefStore::LevelDBPrefStore(
    const base::FilePath& filename,
    base::SequencedTaskRunner* sequenced_task_runner)
    : path_(filename),
      sequenced_task_runner_(sequenced_task_runner),
      original_task_runner_(base::MessageLoopProxy::current()),
      read_only_(false),
      initialized_(false),
      read_error_(PREF_READ_ERROR_NONE),
      weak_ptr_factory_(this) {}

LevelDBPrefStore::~LevelDBPrefStore() {
  CommitPendingWrite();
  sequenced_task_runner_->DeleteSoon(FROM_HERE, serializer_.release());
}

bool LevelDBPrefStore::GetValue(const std::string& key,
                                const base::Value** result) const {
  DCHECK(initialized_);
  const base::Value* tmp = NULL;
  if (!prefs_.GetValue(key, &tmp)) {
    return false;
  }

  if (result)
    *result = tmp;
  return true;
}

// Callers of GetMutableValue have to also call ReportValueChanged.
bool LevelDBPrefStore::GetMutableValue(const std::string& key,
                                       base::Value** result) {
  DCHECK(initialized_);
  return prefs_.GetValue(key, result);
}

void LevelDBPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void LevelDBPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool LevelDBPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool LevelDBPrefStore::IsInitializationComplete() const { return initialized_; }

void LevelDBPrefStore::PersistFromUIThread() {
  if (read_only_)
    return;
  DCHECK(serializer_);

  scoped_ptr<std::set<std::string> > keys_to_delete(new std::set<std::string>);
  keys_to_delete->swap(keys_to_delete_);

  scoped_ptr<std::map<std::string, std::string> > keys_to_set(
      new std::map<std::string, std::string>);
  keys_to_set->swap(keys_to_set_);

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LevelDBPrefStore::FileThreadSerializer::WriteToDatabase,
                 base::Unretained(serializer_.get()),
                 base::Owned(keys_to_set.release()),
                 base::Owned(keys_to_delete.release())));
}

void LevelDBPrefStore::ScheduleWrite() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(10),
                 this,
                 &LevelDBPrefStore::PersistFromUIThread);
  }
}

void LevelDBPrefStore::SetValue(const std::string& key, base::Value* value) {
  SetValueInternal(key, value, true /*notify*/);
}

void LevelDBPrefStore::SetValueSilently(const std::string& key,
                                        base::Value* value) {
  SetValueInternal(key, value, false /*notify*/);
}

static std::string Serialize(base::Value* value) {
  std::string value_string;
  JSONStringValueSerializer serializer(&value_string);
  bool serialized_ok = serializer.Serialize(*value);
  DCHECK(serialized_ok);
  return value_string;
}

void LevelDBPrefStore::SetValueInternal(const std::string& key,
                                        base::Value* value,
                                        bool notify) {
  DCHECK(initialized_);
  DCHECK(value);
  scoped_ptr<base::Value> new_value(value);
  base::Value* old_value = NULL;
  prefs_.GetValue(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    std::string value_string = Serialize(value);
    prefs_.SetValue(key, new_value.release());
    MarkForInsertion(key, value_string);
    if (notify)
      NotifyObservers(key);
  }
}

void LevelDBPrefStore::RemoveValue(const std::string& key) {
  DCHECK(initialized_);
  if (prefs_.RemoveValue(key)) {
    MarkForDeletion(key);
    NotifyObservers(key);
  }
}

bool LevelDBPrefStore::ReadOnly() const { return read_only_; }

PersistentPrefStore::PrefReadError LevelDBPrefStore::GetReadError() const {
  return read_error_;
}

PersistentPrefStore::PrefReadError LevelDBPrefStore::ReadPrefs() {
  DCHECK(!initialized_);
  scoped_ptr<ReadingResults> reading_results;
  if (path_.empty()) {
    reading_results.reset(new ReadingResults);
    reading_results->error = FILE_NOT_SPECIFIED;
  } else {
    reading_results = DoReading(path_);
  }

  PrefReadError error = IntToPrefReadError(reading_results->error);
  OnStorageRead(reading_results.Pass());
  return error;
}

void LevelDBPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  DCHECK_EQ(false, initialized_);
  error_delegate_.reset(error_delegate);
  if (path_.empty()) {
    scoped_ptr<ReadingResults> reading_results(new ReadingResults);
    reading_results->error = FILE_NOT_SPECIFIED;
    OnStorageRead(reading_results.Pass());
    return;
  }
  PostTaskAndReplyWithResult(sequenced_task_runner_.get(),
                             FROM_HERE,
                             base::Bind(&LevelDBPrefStore::DoReading, path_),
                             base::Bind(&LevelDBPrefStore::OnStorageRead,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBPrefStore::CommitPendingWrite() {
  if (timer_.IsRunning()) {
    timer_.Stop();
    PersistFromUIThread();
  }
}

void LevelDBPrefStore::MarkForDeletion(const std::string& key) {
  if (read_only_)
    return;
  keys_to_delete_.insert(key);
  // Need to erase in case there's a set operation in the same batch that would
  // clobber this delete.
  keys_to_set_.erase(key);
  ScheduleWrite();
}

void LevelDBPrefStore::MarkForInsertion(const std::string& key,
                                        const std::string& value) {
  if (read_only_)
    return;
  keys_to_set_[key] = value;
  // Need to erase in case there's a delete operation in the same batch that
  // would clobber this set.
  keys_to_delete_.erase(key);
  ScheduleWrite();
}

void LevelDBPrefStore::ReportValueChanged(const std::string& key) {
  base::Value* new_value = NULL;
  bool contains_value = prefs_.GetValue(key, &new_value);
  DCHECK(contains_value);
  std::string value_string = Serialize(new_value);
  MarkForInsertion(key, value_string);
  NotifyObservers(key);
}

void LevelDBPrefStore::NotifyObservers(const std::string& key) {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

void LevelDBPrefStore::OnStorageRead(
    scoped_ptr<LevelDBPrefStore::ReadingResults> reading_results) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("LevelDBPrefStore.ReadErrors",
                              reading_results->error);
  read_error_ = IntToPrefReadError(reading_results->error);

  if (reading_results->no_dir) {
    FOR_EACH_OBSERVER(
        PrefStore::Observer, observers_, OnInitializationCompleted(false));
    return;
  }

  initialized_ = true;

  if (reading_results->db) {
    DCHECK(reading_results->value_map);
    serializer_.reset(new FileThreadSerializer(reading_results->db.Pass()));
    prefs_.Swap(reading_results->value_map.get());
  } else {
    read_only_ = true;
  }

  // TODO(dgrogan): Call pref_filter_->FilterOnLoad

  if (error_delegate_.get() && read_error_ != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(read_error_);

  FOR_EACH_OBSERVER(
      PrefStore::Observer, observers_, OnInitializationCompleted(true));
}
