// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_H_

#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace leveldb_proto {

class ProtoLevelDBWrapper;

class Enums {
 public:
  enum InitStatus {
    kError = -1,
    kNotInitialized = 0,
    kOK = 1,
    kCorrupt = 2,
    kInvalidOperation = 3,
  };
};

class Callbacks {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;
  using InitStatusCallback = base::OnceCallback<void(Enums::InitStatus)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;
  using LoadKeysCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::vector<std::string>>)>;
  using DestroyCallback = base::OnceCallback<void(bool)>;
  using OnCreateCallback = base::OnceCallback<void(ProtoLevelDBWrapper*)>;

  template <typename T>
  class Internal {
   public:
    using LoadCallback =
        base::OnceCallback<void(bool, std::unique_ptr<std::vector<T>>)>;
    using GetCallback = base::OnceCallback<void(bool, std::unique_ptr<T>)>;
    using LoadKeysAndEntriesCallback =
        base::OnceCallback<void(bool,
                                std::unique_ptr<std::map<std::string, T>>)>;
  };
};

class Util {
 public:
  template <typename T>
  class Internal {
   public:
    // A list of key-value (string, T) tuples.
    using KeyEntryVector = std::vector<std::pair<std::string, T>>;
  };
};

using KeyFilter = base::RepeatingCallback<bool(const std::string& key)>;

// Interface for classes providing persistent storage of Protocol Buffer
// entries (T must be a Proto type extending MessageLite).
template <typename T>
class ProtoDatabase {
 public:
  // For compatibility:
  using KeyEntryVector = typename Util::Internal<T>::KeyEntryVector;

  virtual ~ProtoDatabase() = default;

  // Asynchronously initializes the object with the specified |options|.
  // |callback| will be invoked on the calling thread when complete.
  virtual void Init(const std::string& client_name,
                    Callbacks::InitStatusCallback callback) = 0;
  // This version of Init is for compatibility, since many of the current
  // proto database clients still use this.
  virtual void Init(const char* client_name,
                    const base::FilePath& database_dir,
                    const leveldb_env::Options& options,
                    Callbacks::InitCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |keys_to_remove| from the database. |callback| will be invoked on the
  // calling thread when complete.
  virtual void UpdateEntries(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      Callbacks::UpdateCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries that satisfies
  // the |delete_key_filter| from the database. |callback| will be invoked on
  // the calling thread when complete. The filter will be called on
  // ProtoDatabase's taskrunner.
  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const leveldb_proto::KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) = 0;
  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const leveldb_proto::KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(
      typename Callbacks::Internal<T>::LoadCallback callback) = 0;

  // Asynchronously loads entries that satisfies the |filter| from the database
  // and invokes |callback| when complete. The filter will be called on
  // ProtoDatabase's taskrunner.
  virtual void LoadEntriesWithFilter(
      const leveldb_proto::KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadCallback callback) = 0;
  virtual void LoadEntriesWithFilter(
      const leveldb_proto::KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback) = 0;

  virtual void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;

  virtual void LoadKeysAndEntriesWithFilter(
      const leveldb_proto::KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;
  virtual void LoadKeysAndEntriesWithFilter(
      const leveldb_proto::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;
  // Asynchronously loads entries and their keys for keys in range [start, end]
  // (both inclusive) and invokes |callback| when complete.
  // Range is defined as |start| <= returned keys <= |end|.
  // When |start| = 'bar' and |end| = 'foo' then the keys within brackets are
  // returned: baa, [bar, bara, barb, foa, foo], fooa, fooz, fop.
  virtual void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) = 0;

  // Asynchronously loads all keys from the database and invokes |callback| with
  // those keys when complete.
  virtual void LoadKeys(typename Callbacks::LoadKeysCallback callback) = 0;
  virtual void LoadKeys(const std::string& target_prefix,
                        typename Callbacks::LoadKeysCallback callback) = 0;

  // Asynchronously loads a single entry, identified by |key|, from the database
  // and invokes |callback| when complete. If no entry with |key| is found,
  // a nullptr is passed to the callback, but the success flag is still true.
  virtual void GetEntry(
      const std::string& key,
      typename Callbacks::Internal<T>::GetCallback callback) = 0;

  // Asynchronously destroys the database. Use this call only if the database
  // needs to be destroyed for this particular profile. If the database is no
  // longer useful for everyone, the client name must be added to
  // |kObsoleteSharedProtoDatabaseClients| to ensure automatic clean up of the
  // database from all users.
  virtual void Destroy(Callbacks::DestroyCallback callback) = 0;

 protected:
  ProtoDatabase() = default;
};

// Return a new instance of Options, but with two additions:
// 1) create_if_missing = true
// 2) max_open_files = 0
leveldb_env::Options CreateSimpleOptions();

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_H_