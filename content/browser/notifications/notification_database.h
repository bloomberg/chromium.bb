// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"

namespace leveldb {
class DB;
class Env;
}

namespace content {

// Implementation of the persistent notification database.
//
// This class can be constructed on any thread, but method calls must only be
// made on a thread or sequenced task runner that allows file I/O. The same
// thread or task runner must be used for all method calls.
class CONTENT_EXPORT NotificationDatabase {
 public:
  // Result status codes for interations with the database. Will be used for
  // UMA, so the assigned ids must remain stable.
  enum Status {
    STATUS_OK = 0,

    // The database, or the key associated with the operation, could not be
    // found.
    STATUS_ERROR_NOT_FOUND = 1,

    // The database, or data in the database, could not be parsed as valid data.
    STATUS_ERROR_CORRUPTED = 2,

    // General failure code. More specific failures should be used if available.
    STATUS_ERROR_FAILED = 3,
  };

  explicit NotificationDatabase(const base::FilePath& path);
  ~NotificationDatabase();

  // Opens the database. If |path| is non-empty, it will be created on the given
  // directory on the filesystem. If |path| is empty, the database will be
  // created in memory instead, and its lifetime will be tied to this instance.
  // |create_if_missing| determines whether to create the database if necessary.
  Status Open(bool create_if_missing);

  // Completely destroys the contents of this database.
  Status Destroy();

 private:
  friend class NotificationDatabaseTest;

  // TODO(peter): Convert to an enum class when DCHECK_EQ supports this.
  // See https://crbug.com/463869.
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZED,
    STATE_DISABLED,
  };

  // Returns whether the database has been opened.
  bool IsOpen() const { return db_ != nullptr; }

  // Returns whether the database should only exist in memory.
  bool IsInMemoryDatabase() const { return path_.empty(); }

  base::FilePath path_;

  // The declaration order for these members matters, as |db_| depends on |env_|
  // and thus has to be destructed first.
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;

  State state_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_
