// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/v4_store.h"

namespace safe_browsing {

class V4Database;

typedef base::Callback<void(std::unique_ptr<V4Database>)>
    NewDatabaseReadyCallback;

typedef const base::hash_map<UpdateListIdentifier,
                             const base::FilePath::CharType>
    ListInfoMap;

typedef std::unique_ptr<
    base::hash_map<UpdateListIdentifier, std::unique_ptr<V4Store>>>
    StoreMap;

// Factory for creating V4Database. Tests implement this factory to create fake
// databases for testing.
class V4DatabaseFactory {
 public:
  virtual ~V4DatabaseFactory() {}
  virtual V4Database* CreateV4Database(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      ListInfoMap list_info_map) = 0;
};

// The on-disk databases are shared among all profiles, as it doesn't contain
// user-specific data. This object is not thread-safe, i.e. all its methods
// should be used on the same thread that it was created on, unless specified
// otherwise.
// The hash-prefixes of each type are managed by a V4Store (including saving to
// and reading from disk).
// The V4Database serves as a single place to manage all the V4Stores.
class V4Database {
 public:
  // Factory method to create a V4Database. It creates the database on the
  // provided |db_task_runner|. When the database creation is complete, it calls
  // the NewDatabaseReadyCallback on the same thread as it was called.
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      ListInfoMap list_info_map,
      NewDatabaseReadyCallback callback);

  virtual ~V4Database();

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase();

  // Makes the passed |factory| the factory used to instantiate
  // a V4Database. Only for tests.
  static void RegisterFactoryForTest(V4DatabaseFactory* factory) {
    factory_ = factory;
  }

 protected:
  V4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
             StoreMap store_map);

 private:
  // Factory method to create a V4Database. When the database creation is
  // complete, it calls the NewDatabaseReadyCallback on |callback_task_runner|.
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      ListInfoMap list_info_map,
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback callback);

  const scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Map of UpdateListIdentifier to the V4Store.
  StoreMap store_map_;

  // The factory that controls the creation of V4Database objects.
  // This is used *only* by tests.
  static V4DatabaseFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(V4Database);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
