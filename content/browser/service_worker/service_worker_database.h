// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace leveldb {
class DB;
class Env;
class Slice;
class WriteBatch;
}

namespace content {

// Class to persist serviceworker regitration data in a database.
// Should NOT be used on the IO thread since this does blocking
// file io. The ServiceWorkerStorage class owns this class and
// is responsible for only calling it serially on background
// nonIO threads (ala SequencedWorkerPool).
class CONTENT_EXPORT ServiceWorkerDatabase {
 public:
  // We do leveldb stuff in |path| or in memory if |path| is empty.
  explicit ServiceWorkerDatabase(const base::FilePath& path);
  ~ServiceWorkerDatabase();

  struct RegistrationData {
    // These values are immutable for the life of a registration.
    int64 registration_id;
    GURL scope;
    GURL script;

    // Versions are first stored once they successfully install and become
    // the waiting version. Then transition to the active version. The stored
    // version may be in the ACTIVE state or in the INSTALLED state.
    int64 version_id;
    bool is_active;
    bool has_fetch_handler;
    base::Time last_update_check;

    ServiceWorkerVersion::Status GetVersionStatus() const {
      if (is_active)
        return ServiceWorkerVersion::ACTIVE;
      return ServiceWorkerVersion::INSTALLED;
    }

    RegistrationData();
    ~RegistrationData();
  };

  // For use during initialization.
  bool GetNextAvailableIds(int64* next_avail_registration_id,
                           int64* next_avail_version_id,
                           int64* next_avail_resource_id);

  bool is_disabled() const { return is_disabled_; }
  bool was_corruption_detected() const { return was_corruption_detected_; }

 private:
  // Opens the database at the |path_|. This is lazily called when the first
  // database API is called. Returns true if the database was opened. Returns
  // false if the opening failed or was not neccessary, that is, the database
  // does not exist and |create_if_needed| is false.
  bool LazyOpen(bool create_if_needed);

  // Populates the database with initial data, namely, database schema version
  // and next available IDs.
  bool PopulateInitialData();

  bool ReadInt64(const leveldb::Slice& key, int64* value_out);
  bool WriteBatch(scoped_ptr<leveldb::WriteBatch> batch);

  bool IsOpen();
  bool IsEmpty();

  base::FilePath path_;
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;

  // True if a database error has occurred (e.g. cannot read data).
  // If true, all database accesses will fail.
  bool is_disabled_;

  // True if a database corruption was detected.
  bool was_corruption_detected_;

  base::SequenceChecker sequence_checker_;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase_InMemory);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, GetNextAvailableIds);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
