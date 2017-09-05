// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_DATABASE_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_DATABASE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

class GURL;

namespace leveldb {
class DB;
struct ReadOptions;
class WriteBatch;
}  // namespace leveldb

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace content {

// SessionStorageDatabase holds the data from multiple namespaces and multiple
// origins. All DOMStorageAreas for session storage share the same
// SessionStorageDatabase.

// Only one thread is allowed to call the public functions other than
// ReadAreaValues and ReadNamespacesAndOrigins. Other threads are allowed to
// call ReadAreaValues and ReadNamespacesAndOrigins.
class CONTENT_EXPORT SessionStorageDatabase :
    public base::RefCountedThreadSafe<SessionStorageDatabase> {
 public:
  explicit SessionStorageDatabase(const base::FilePath& file_path);

  // Reads the (key, value) pairs for |namespace_id| and |origin|. |result| is
  // assumed to be empty and any duplicate keys will be overwritten. If the
  // database exists on disk then it will be opened. If it does not exist then
  // it will not be created and |result| will be unmodified.
  void ReadAreaValues(
      const std::string& namespace_id,
      const std::vector<std::string>& original_permanent_namespace_ids,
      const GURL& origin,
      DOMStorageValuesMap* result);

  // Updates the data for |namespace_id| and |origin|. Will remove all keys
  // before updating the database if |clear_all_first| is set. Then all entries
  // in |changes| will be examined - keys mapped to a null NullableString16 will
  // be removed and all others will be inserted/updated as appropriate. It is
  // allowed to write data into a shallow copy created by CloneNamespace, and in
  // that case the copy will be made deep before writing the values.
  bool CommitAreaChanges(const std::string& namespace_id,
                         const GURL& origin,
                         bool clear_all_first,
                         const DOMStorageValuesMap& changes);

  // Creates shallow copies of the areas for |namespace_id| and associates them
  // with |new_namespace_id|.
  bool CloneNamespace(const std::string& namespace_id,
                      const std::string& new_namespace_id);

  // Deletes the data for |namespace_id| and |origin|.
  bool DeleteArea(const std::string& namespace_id, const GURL& origin);

  // Deletes the data for |namespace_id|.
  bool DeleteNamespace(const std::string& namespace_id);

  // Reads the namespace IDs and origins present in the database.
  bool ReadNamespacesAndOrigins(
      std::map<std::string, std::vector<GURL> >* namespaces_and_origins);

  // Adds memory statistics to |pmd| for chrome://tracing.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd);

 private:
  friend class base::RefCountedThreadSafe<SessionStorageDatabase>;
  class DBOperation;
  friend class SessionStorageDatabase::DBOperation;
  friend class SessionStorageDatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaParamTest, ShallowCopyWithBacking);

  ~SessionStorageDatabase();

  // Opens the database at file_path_ if it exists already and creates it if
  // |create_if_needed| is true. Returns true if the database was opened, false
  // if the opening failed or was not necessary (the database doesn't exist and
  // |create_if_needed| is false). The possible failures are:
  // - leveldb cannot open the database.
  // - The database is in an inconsistent or errored state.
  bool LazyOpen(bool create_if_needed);

  // Tries to open the database at file_path_, assigns |db| to point to the
  // opened leveldb::DB instance.
  leveldb::Status TryToOpen(std::unique_ptr<leveldb::DB>* db);

  // Returns true if the database is already open, false otherwise.
  bool IsOpen() const;

  // Helpers for checking caller erros, invariants and database errors. All
  // these return |ok|, for chaining.
  bool CallerErrorCheck(bool ok) const;
  bool ConsistencyCheck(bool ok);
  bool DatabaseErrorCheck(bool ok);

  // Helper functions. All return true if the operation succeeded, and false if
  // it failed (a database error or a consistency error). If the return type is
  // void, the operation cannot fail. If they return false, ConsistencyCheck or
  // DatabaseErrorCheck have already been called.

  // Creates a namespace for |namespace_id| and updates the next namespace id if
  // needed. If |ok_if_exists| is false, checks that the namespace didn't exist
  // before.
  bool CreateNamespace(const std::string& namespace_id,
                       bool ok_if_exists,
                       leveldb::WriteBatch* batch);

  // Reads the areas assoiated with |namespace_id| and puts the (origin, map_id)
  // pairs into |areas|.
  bool GetAreasInNamespace(const std::string& namespace_id,
                           std::map<std::string, std::string>* areas);

  // Adds an association between |origin| and |map_id| into the namespace
  // |namespace_id|.
  void AddAreaToNamespace(const std::string& namespace_id,
                          const std::string& origin,
                          const std::string& map_id,
                          leveldb::WriteBatch* batch);

  // Helpers for deleting data for |namespace_id| and |origin|.
  bool DeleteAreaHelper(const std::string& namespace_id,
                        const std::string& origin,
                        leveldb::WriteBatch* batch);

  // Retrieves the map id for |namespace_id| and |origin|. It's not an error if
  // the map doesn't exist.
  bool GetMapForArea(const std::string& namespace_id,
                     const std::string& origin,
                     const leveldb::ReadOptions& options,
                     bool* exists,
                     std::string* map_id);

  // Creates a new map for |namespace_id| and |origin|. |map_id| will hold the
  // id of the created map. If there is a map for |namespace_id| and |origin|,
  // this just overwrites the map id. The caller is responsible for decreasing
  // the ref count.
  bool CreateMapForArea(const std::string& namespace_id,
                        const GURL& origin,
                        std::string* map_id,
                        leveldb::WriteBatch* batch);
  // Reads the contents of the map |map_id| into |result|. If |only_keys| is
  // true, only keys are aread from the database and the values in |result| will
  // be empty.
  bool ReadMap(const std::string& map_id,
               const leveldb::ReadOptions& options,
               DOMStorageValuesMap* result,
               bool only_keys);
  // Writes |values| into the map |map_id|.
  void WriteValuesToMap(const std::string& map_id,
                        const DOMStorageValuesMap& values,
                        leveldb::WriteBatch* batch);

  bool GetMapRefCount(const std::string& map_id, int64_t* ref_count);
  bool IncreaseMapRefCount(const std::string& map_id,
                           leveldb::WriteBatch* batch);
  // Decreases the ref count of a map by |decrease|. If the ref count goes to 0,
  // deletes the map.
  bool DecreaseMapRefCount(const std::string& map_id,
                           int decrease,
                           leveldb::WriteBatch* batch);

  // Deletes all values in |map_id|.
  bool ClearMap(const std::string& map_id, leveldb::WriteBatch* batch);

  // Breaks the association between (|namespace_id|, |origin|) and |map_id| and
  // creates a new map for (|namespace_id|, |origin|). Copies the data from the
  // old map if |copy_data| is true.
  bool DeepCopyArea(const std::string& namespace_id,
                    const GURL& origin,
                    bool copy_data,
                    std::string* map_id,
                    leveldb::WriteBatch* batch);

  // Helper functions for creating the keys needed for the schema.
  static std::string NamespaceStartKey(const std::string& namespace_id);
  static std::string NamespaceKey(const std::string& namespace_id,
                                  const std::string& origin);
  static const char* NamespacePrefix();
  static std::string MapRefCountKey(const std::string& map_id);
  static std::string MapKey(const std::string& map_id, const std::string& key);
  static const char* NextMapIdKey();

  std::unique_ptr<leveldb::DB> db_;
  base::FilePath file_path_;

  // For protecting the database opening code. Also guards the variables below.
  base::Lock db_lock_;

  // True if a database error has occurred (e.g., cannot read data).
  bool db_error_;
  // True if the database is in an inconsistent state.
  bool is_inconsistent_;
  // True if the database is in a failed or inconsistent state, and we have
  // already deleted it (as an attempt to recover later).
  bool invalid_db_deleted_;

  // The number of database operations in progress. We need this so that we can
  // delete an inconsistent database at the right moment.
  int operation_count_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_DATABASE_H_
