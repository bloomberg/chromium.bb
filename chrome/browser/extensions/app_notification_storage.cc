// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification_storage.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using base::JSONReader;
using base::JSONWriter;

// A concrete implementation of the AppNotificationStorage interface, using
// LevelDb for backing storage.
class LevelDbAppNotificationStorage : public AppNotificationStorage {
 public:
  explicit LevelDbAppNotificationStorage(const FilePath& path);
  virtual ~LevelDbAppNotificationStorage();

  // Must be called exactly once before any other operations. This will return
  // false if any condition would prevent further operations from being
  // successful (eg unable to create files at |path_|).
  bool Initialize();

  // Implementing the AppNotificationStorage interface.
  virtual bool GetExtensionIds(std::set<std::string>* result) OVERRIDE;
  virtual bool Get(const std::string& extension_id,
                   AppNotificationList* result) OVERRIDE;
  virtual bool Set(const std::string& extension_id,
                   const AppNotificationList& list) OVERRIDE;
  virtual bool Delete(const std::string& extension_id) OVERRIDE;

 private:
  // The path where the database will reside.
  FilePath path_;

  // This should be used for all read operations on the db.
  leveldb::ReadOptions read_options_;

  // The leveldb database object - will be NULL until Initialize is called.
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(LevelDbAppNotificationStorage);
};

// static
AppNotificationStorage* AppNotificationStorage::Create(
    const FilePath& path) {
  scoped_ptr<LevelDbAppNotificationStorage> result(
      new LevelDbAppNotificationStorage(path));
  if (!result->Initialize())
    return NULL;
  return result.release();
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
  JSONWriter::Write(&list_value, false /* pretty_print */, result);
}

bool JSONToAppNotificationList(const std::string& json,
                               AppNotificationList* list) {
  CHECK(list);
  scoped_ptr<Value> value(JSONReader::Read(json,
                                           false /* allow_trailing_comma */));
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

void LogLevelDbError(tracked_objects::Location location,
                     const leveldb::Status& status) {
  LOG(ERROR) << "AppNotificationStorage database error at "
             << location.ToString() << " status:" << status.ToString();
}

}  // namespace


LevelDbAppNotificationStorage::LevelDbAppNotificationStorage(
    const FilePath& path) : path_(path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  read_options_.verify_checksums = true;
}

LevelDbAppNotificationStorage::~LevelDbAppNotificationStorage() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

bool LevelDbAppNotificationStorage::Initialize() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(!db_.get());
  CHECK(!path_.empty());

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

bool LevelDbAppNotificationStorage::GetExtensionIds(
    std::set<std::string>* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(db_.get());
  CHECK(result);

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
  CHECK(db_.get());
  CHECK(result);

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
  CHECK(db_.get());

  // Leveldb does not consider it an error if the key to delete isn't present,
  // so we don't bother checking that first.
  leveldb::Status status = db_->Delete(leveldb::WriteOptions(), extension_id);

  if (!status.ok()) {
    LogLevelDbError(FROM_HERE, status);
    return false;
  }
  return true;
}
