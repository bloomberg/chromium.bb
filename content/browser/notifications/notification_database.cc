// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include "base/files/file_util.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace content {
namespace {

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

}  // namespace

NotificationDatabase::NotificationDatabase(const base::FilePath& path)
    : path_(path),
      state_(STATE_UNINITIALIZED) {
  sequence_checker_.DetachFromSequence();
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

  return STATUS_OK;
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

}  // namespace content
