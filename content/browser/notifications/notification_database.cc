// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include <string>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/notifications/notification_database_data.h"
#include "content/public/browser/browser_thread.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

// Notification LevelDB database schema (in alphabetized order)
// =======================
//
// key: "DATA:" <origin identifier> '\x00' <notification_id>
// value: String containing the NotificationDatabaseDataProto protocol buffer
//        in serialized form.
//
// key: "NEXT_NOTIFICATION_ID"
// value: Decimal string which fits into an int64_t.
//

namespace content {
namespace {

// Keys of the fields defined in the database.
const char kNextNotificationIdKey[] = "NEXT_NOTIFICATION_ID";
const char kDataKeyPrefix[] = "DATA:";

// Separates the components of compound keys.
const char kKeySeparator = '\x00';

// The first notification id which to be handed out by the database.
const int64_t kFirstNotificationId = 1;

// Converts the LevelDB |status| to one of the notification database's values.
NotificationDatabase::Status LevelDBStatusToStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return NotificationDatabase::STATUS_OK;
  else if (status.IsNotFound())
    return NotificationDatabase::STATUS_ERROR_NOT_FOUND;
  else if (status.IsCorruption())
    return NotificationDatabase::STATUS_ERROR_CORRUPTED;

  return NotificationDatabase::STATUS_ERROR_FAILED;
}

// Creates the compound data key in which notification data is stored.
std::string CreateDataKey(int64_t notification_id, const GURL& origin) {
  return base::StringPrintf("%s%s%c%s",
                            kDataKeyPrefix,
                            storage::GetIdentifierFromOrigin(origin).c_str(),
                            kKeySeparator,
                            base::Int64ToString(notification_id).c_str());
}

}  // namespace

NotificationDatabase::NotificationDatabase(const base::FilePath& path)
    : path_(path) {
}

NotificationDatabase::~NotificationDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

NotificationDatabase::Status NotificationDatabase::Open(
    bool create_if_missing) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_UNINITIALIZED, state_);

  if (!create_if_missing) {
    if (IsInMemoryDatabase() ||
        !base::PathExists(path_) ||
        base::IsDirectoryEmpty(path_)) {
      return NotificationDatabase::STATUS_ERROR_NOT_FOUND;
    }
  }

  leveldb::Options options;
  options.create_if_missing = create_if_missing;
  options.paranoid_checks = true;
  if (IsInMemoryDatabase()) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  }

  leveldb::DB* db = nullptr;
  Status status = LevelDBStatusToStatus(
      leveldb::DB::Open(options, path_.AsUTF8Unsafe(), &db));
  if (status != STATUS_OK)
    return status;

  state_ = STATE_INITIALIZED;
  db_.reset(db);

  return ReadNextNotificationId();
}

NotificationDatabase::Status NotificationDatabase::ReadNotificationData(
    int64_t notification_id,
    const GURL& origin,
    NotificationDatabaseData* notification_database_data) const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_INITIALIZED, state_);
  DCHECK_GE(notification_id, kFirstNotificationId);
  DCHECK(origin.is_valid());
  DCHECK(notification_database_data);

  std::string key = CreateDataKey(notification_id, origin);
  std::string serialized_data;

  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), key, &serialized_data));
  if (status != STATUS_OK)
    return status;

  if (notification_database_data->ParseFromString(serialized_data))
    return STATUS_OK;

  DLOG(ERROR) << "Unable to deserialize data for notification "
              << notification_id << " belonging to " << origin << ".";
  return STATUS_ERROR_CORRUPTED;
}

NotificationDatabase::Status NotificationDatabase::WriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& notification_database_data,
    int64_t* notification_id) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_INITIALIZED, state_);
  DCHECK(notification_id);
  DCHECK(origin.is_valid());

  std::string serialized_data;
  if (!notification_database_data.SerializeToString(&serialized_data)) {
    DLOG(ERROR) << "Unable to serialize data for a notification belonging "
                << "to: " << origin;
    return STATUS_ERROR_FAILED;
  }

  DCHECK_GE(next_notification_id_, kFirstNotificationId);

  leveldb::WriteBatch batch;
  batch.Put(CreateDataKey(next_notification_id_, origin), serialized_data);
  batch.Put(kNextNotificationIdKey,
            base::Int64ToString(next_notification_id_ + 1));

  Status status = LevelDBStatusToStatus(
      db_->Write(leveldb::WriteOptions(), &batch));
  if (status != STATUS_OK)
    return status;

  *notification_id = next_notification_id_++;
  return STATUS_OK;
}

NotificationDatabase::Status NotificationDatabase::DeleteNotificationData(
    int64_t notification_id,
    const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_INITIALIZED, state_);
  DCHECK_GE(notification_id, kFirstNotificationId);
  DCHECK(origin.is_valid());

  std::string key = CreateDataKey(notification_id, origin);
  return LevelDBStatusToStatus(db_->Delete(leveldb::WriteOptions(), key));
}

NotificationDatabase::Status NotificationDatabase::Destroy() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  leveldb::Options options;
  if (IsInMemoryDatabase()) {
    if (!env_)
      return STATUS_OK;  // The database has not been initialized.

    options.env = env_.get();
  }

  state_ = STATE_DISABLED;
  db_.reset();

  return LevelDBStatusToStatus(
      leveldb::DestroyDB(path_.AsUTF8Unsafe(), options));
}

NotificationDatabase::Status NotificationDatabase::ReadNextNotificationId() {
  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), kNextNotificationIdKey, &value));

  if (status == STATUS_ERROR_NOT_FOUND) {
    next_notification_id_ = kFirstNotificationId;
    return STATUS_OK;
  }

  if (status != STATUS_OK)
    return status;

  if (!base::StringToInt64(value, &next_notification_id_) ||
      next_notification_id_ < kFirstNotificationId) {
    return STATUS_ERROR_CORRUPTED;
  }

  return STATUS_OK;
}

}  // namespace content
