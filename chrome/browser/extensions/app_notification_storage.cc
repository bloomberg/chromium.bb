// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification_storage.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using base::JSONReader;
using base::JSONWriter;
using content::BrowserThread;

namespace extensions {

// A concrete implementation of the AppNotificationStorage interface, using
// LevelDb for backing storage.
class LevelDbAppNotificationStorage : public AppNotificationStorage {
 public:
  explicit LevelDbAppNotificationStorage(const base::FilePath& path);
  virtual ~LevelDbAppNotificationStorage();

  // Implementing the AppNotificationStorage interface.
  virtual bool GetExtensionIds(std::set<std::string>* result) OVERRIDE;
  virtual bool Get(const std::string& extension_id,
                   AppNotificationList* result) OVERRIDE;
  virtual bool Set(const std::string& extension_id,
                   const AppNotificationList& list) OVERRIDE;
  virtual bool Delete(const std::string& extension_id) OVERRIDE;

 private:
  // If |db_| is NULL, attempt to open it. You should pass true for
  // |create_if_missing| if you need to write data into the database and want to
  // ensure it has been created after this call.
  bool OpenDbIfNeeded(bool create_if_missing);

  // The path where the database will reside.
  base::FilePath path_;

  // This should be used for all read operations on the db.
  leveldb::ReadOptions read_options_;

  // The leveldb database object - might be NULL if there was nothing found on
  // disk at |path_| and OpenDbIfNeeded hasn't been called with
  // create_if_missing=true yet.
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(LevelDbAppNotificationStorage);
};

// static
AppNotificationStorage* AppNotificationStorage::Create(
    const base::FilePath& path) {
  return new LevelDbAppNotificationStorage(path);
}

AppNotificationStorage::~AppNotificationStorage() {}

namespace {

void AppNotificationListToJSON(const AppNotificationList& list,
                               std::string* result) {
  ListValue list_value;
  AppNotificationList::const_iterator i;
  for (i = list.begin(); i != list.end(); ++i) {
    DictionaryValue* dictionary = new DictionaryValue();
    (*i)->ToDictionaryValue(dictionary);
    list_value.Append(dictionary);
  }
  JSONWriter::Write(&list_value, result);
}

bool JSONToAppNotificationList(const std::string& json,
                               AppNotificationList* list) {
  CHECK(list);
  scoped_ptr<Value> value(JSONReader::Read(json));
  if (!value.get() || value->GetType() != Value::TYPE_LIST)
    return false;

  ListValue* list_value = static_cast<ListValue*>(value.get());
  for (size_t i = 0; i < list_value->GetSize(); i++) {
    Value* item = NULL;
    if (!list_value->Get(i, &item) || !item ||
        item->GetType() != Value::TYPE_DICTIONARY)
      return false;
    DictionaryValue* dictionary = static_cast<DictionaryValue*>(item);
    AppNotification* notification =
        AppNotification::FromDictionaryValue(*dictionary);
    if (!notification)
      return false;
    list->push_back(linked_ptr<AppNotification>(notification));
  }
  return true;
}

void LogLevelDbError(const tracked_objects::Location& location,
                     const leveldb::Status& status) {
  LOG(ERROR) << "AppNotificationStorage database error at "
             << location.ToString() << " status:" << status.ToString();
}

}  // namespace


LevelDbAppNotificationStorage::LevelDbAppNotificationStorage(
    const base::FilePath& path) : path_(path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  read_options_.verify_checksums = true;
}

LevelDbAppNotificationStorage::~LevelDbAppNotificationStorage() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

bool LevelDbAppNotificationStorage::GetExtensionIds(
    std::set<std::string>* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(result);
  if (!OpenDbIfNeeded(false))
    return false;
  if (!db_.get())
    return true;

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options_));
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    std::string key = iter->key().ToString();
    if (Extension::IdIsValid(key))
      result->insert(key);
  }

  return true;
}

bool LevelDbAppNotificationStorage::Get(const std::string& extension_id,
                                        AppNotificationList* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(result);
  if (!OpenDbIfNeeded(false))
    return false;
  if (!db_.get())
    return true;

  std::string json;
  leveldb::Status status = db_->Get(read_options_, extension_id, &json);
  if (status.IsNotFound()) {
    return true;
  } else if (!status.ok()) {
    LogLevelDbError(FROM_HERE, status);
    return false;
  }

  return JSONToAppNotificationList(json, result);
}

bool LevelDbAppNotificationStorage::Set(const std::string& extension_id,
                                        const AppNotificationList& list) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!OpenDbIfNeeded(true))
    return false;
  CHECK(db_.get());

  std::string json;
  AppNotificationListToJSON(list, &json);
  leveldb::Status status = db_->Put(leveldb::WriteOptions(),
                                    extension_id,
                                    json);
  if (!status.ok()) {
    LogLevelDbError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool LevelDbAppNotificationStorage::Delete(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!OpenDbIfNeeded(false))
    return false;

  if (!db_.get())
    return true; // The database doesn't exist on disk.

  // Leveldb does not consider it an error if the key to delete isn't present,
  // so we don't bother checking that first.
  leveldb::Status status = db_->Delete(leveldb::WriteOptions(), extension_id);

  if (!status.ok()) {
    LogLevelDbError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool LevelDbAppNotificationStorage::OpenDbIfNeeded(bool create_if_missing) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (db_.get())
    return true;

  // If the file doesn't exist and the caller doesn't want it created, just
  // return early.
  if (!create_if_missing && !file_util::PathExists(path_))
    return true;

#if defined(OS_POSIX)
  std::string os_path = path_.value();
#elif defined(OS_WIN)
  std::string os_path = base::SysWideToUTF8(path_.value());
#endif

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, os_path, &db);
  if (!status.ok()) {
    LogLevelDbError(FROM_HERE, status);
    return false;
  }
  db_.reset(db);
  return true;
}

}  // namespace extensions
