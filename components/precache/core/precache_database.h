// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace sql {
class Connection;
}

namespace precache {

class PrecacheURLTable;

// Class that tracks information related to precaching. This class can be
// constructed or destroyed on any threads, but all other methods must be called
// on the same thread (e.g. the DB thread).
class PrecacheDatabase : public base::RefCountedThreadSafe<PrecacheDatabase> {
 public:
  // A PrecacheDatabase can be constructed on any thread.
  PrecacheDatabase();

  // Initializes the precache database, using the specified database file path.
  // Init must be called before any other methods.
  bool Init(const base::FilePath& db_path);

  // Deletes precache history from the precache URL table that is more than 60
  // days older than |current_time|.
  void DeleteExpiredPrecacheHistory(const base::Time& current_time);

  // Report precache-related metrics in response to a URL being fetched, where
  // the fetch was motivated by precaching.
  void RecordURLPrecached(const GURL& url, const base::Time& fetch_time,
                          int64 size, bool was_cached);

  // Report precache-related metrics in response to a URL being fetched, where
  // the fetch was not motivated by precaching. |is_connection_cellular|
  // indicates whether the current network connection is a cellular network.
  void RecordURLFetched(const GURL& url, const base::Time& fetch_time,
                        int64 size, bool was_cached,
                        bool is_connection_cellular);

 private:
  friend class base::RefCountedThreadSafe<PrecacheDatabase>;
  friend class PrecacheDatabaseTest;

  ~PrecacheDatabase();

  bool IsDatabaseAccessible() const;

  scoped_ptr<sql::Connection> db_;

  // Table that keeps track of URLs that are in the cache because of precaching,
  // and wouldn't be in the cache otherwise.
  scoped_ptr<PrecacheURLTable> precache_url_table_;

  // ThreadChecker used to ensure that all methods other than the constructor
  // or destructor are called on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheDatabase);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_
