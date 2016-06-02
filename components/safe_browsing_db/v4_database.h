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

// This defines a hash_map that is used to create the backing files for the
// stores that contain the hash-prefixes. The map key identifies the list that
// we're interested in and the value represents the ASCII file-name that'll be
// created on-disk to store the hash-prefixes for that list. This file is
// created inside the user's profile directory.
// For instance, the UpdateListIdentifier could be for URL expressions for UwS
// on Windows platform, and the corresponding file on disk could be named:
// "uws_win_url.store"
typedef base::hash_map<UpdateListIdentifier, std::string> ListInfoMap;

// This hash_map maps the UpdateListIdentifiers to their corresponding in-memory
// stores, which contain the hash prefixes for that UpdateListIdentifier as well
// as manage their storage on disk.
typedef base::hash_map<UpdateListIdentifier, std::unique_ptr<V4Store>> StoreMap;

// Factory for creating V4Database. Tests implement this factory to create fake
// databases for testing.
class V4DatabaseFactory {
 public:
  virtual ~V4DatabaseFactory() {}
  virtual V4Database* CreateV4Database(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_dir_path,
      const ListInfoMap& list_info_map) = 0;
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
      const ListInfoMap& list_info_map,
      NewDatabaseReadyCallback callback);

  // Destroys the provided v4_database on its task_runner since this may be a
  // long operation.
  static void Destroy(std::unique_ptr<V4Database> v4_database);

  virtual ~V4Database();

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase();

 protected:
  V4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
             std::unique_ptr<StoreMap> store_map);

 private:
  friend class SafeBrowsingV4DatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4DatabaseTest,
                           TestSetupDatabaseWithFakeStores);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4DatabaseTest,
                           TestSetupDatabaseWithFakeStoresFailsReset);

  // Makes the passed |factory| the factory used to instantiate a V4Store. Only
  // for tests.
  static void RegisterStoreFactoryForTest(V4StoreFactory* factory) {
    factory_ = factory;
  }

  // Factory method to create a V4Database. When the database creation is
  // complete, it calls the NewDatabaseReadyCallback on |callback_task_runner|.
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfoMap& list_info_map,
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback callback);

  const scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Map of UpdateListIdentifier to the V4Store.
  const std::unique_ptr<StoreMap> store_map_;

  // The factory that controls the creation of V4Store objects.
  static V4StoreFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(V4Database);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
