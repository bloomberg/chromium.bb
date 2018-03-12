// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_BACKEND_H_
#define COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_BACKEND_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/sync/model/model_type_store.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class DB;
class Env;
class WriteBatch;
}  // namespace leveldb

namespace syncer {

// Different reasons for ModelTypeStoreBackend initialization failure are mapped
// to these values. The enum is used for recording UMA histogram. Don't reorder,
// change or delete values.
enum StoreInitResultForHistogram {
  STORE_INIT_RESULT_SUCCESS = 0,

  // Following values reflect leveldb initialization errors.
  STORE_INIT_RESULT_NOT_FOUND,
  STORE_INIT_RESULT_CORRUPTION,
  STORE_INIT_RESULT_NOT_SUPPORTED,
  STORE_INIT_RESULT_INVALID_ARGUMENT,
  STORE_INIT_RESULT_IO_ERROR,

  // Issues encountered when reading or parsing schema descriptor.
  STORE_INIT_RESULT_SCHEMA_DESCRIPTOR_ISSUE,

  // Database schema migration failed.
  STORE_INIT_RESULT_MIGRATION,

  STORE_INIT_RESULT_UNKNOWN,

  // Database was reset after attempt to open failed with corruption.
  STORE_INIT_RESULT_RECOVERED_AFTER_CORRUPTION,
  STORE_INIT_RESULT_COUNT
};

// ModelTypeStoreBackend handles operations with leveldb. It is oblivious of the
// fact that it is called from separate thread (with the exception of ctor),
// meaning it shouldn't deal with callbacks and task_runners.
class ModelTypeStoreBackend
    : public base::RefCountedThreadSafe<ModelTypeStoreBackend> {
 public:
  // Helper function to create in memory environment for leveldb.
  static std::unique_ptr<leveldb::Env> CreateInMemoryEnv();

  // GetOrCreateBackend will check if |backend_map_| has a backend with same
  // |path|. If |backend_map_| has one, wrap that backend to scoped_refptr and
  // return. If |backend_map_| does not have, create a backend and store it into
  // |backend_map_|.
  static scoped_refptr<ModelTypeStoreBackend> GetOrCreateBackend(
      const std::string& path,
      std::unique_ptr<leveldb::Env> env,
      base::Optional<ModelError>* error);

  // Reads records with keys formed by prepending ids from |id_list| with
  // |prefix|. If the record is found its id (without prefix) and value is
  // appended to record_list. If record is not found its id is appended to
  // |missing_id_list|. It is not an error that records for ids are not found so
  // function will still return success in this case.
  base::Optional<ModelError> ReadRecordsWithPrefix(
      const std::string& prefix,
      const ModelTypeStore::IdList& id_list,
      ModelTypeStore::RecordList* record_list,
      ModelTypeStore::IdList* missing_id_list);

  // Reads all records with keys starting with |prefix|. Prefix is removed from
  // key before it is added to |record_list|.
  base::Optional<ModelError> ReadAllRecordsWithPrefix(
      const std::string& prefix,
      ModelTypeStore::RecordList* record_list);

  // Writes modifications accumulated in |write_batch| to database.
  base::Optional<ModelError> WriteModifications(
      std::unique_ptr<leveldb::WriteBatch> write_batch);

  base::Optional<ModelError> DeleteDataAndMetadataForPrefix(
      const std::string& prefix);

 private:
  friend class base::RefCountedThreadSafe<ModelTypeStoreBackend>;
  friend class ModelTypeStoreBackendTest;

  static const int64_t kLatestSchemaVersion;
  static const char kDBSchemaDescriptorRecordId[];
  static const char kStoreInitResultHistogramName[];

  explicit ModelTypeStoreBackend(const std::string& path);
  ~ModelTypeStoreBackend();

  // Init opens database at |path|. If database doesn't exist it creates one.
  // Normally |env| should be nullptr, this causes leveldb to use default disk
  // based environment from leveldb::Env::Default().
  // Providing |env| allows to override environment used by leveldb for tests
  // with in-memory or faulty environment.
  base::Optional<ModelError> Init(const std::string& path,
                                  std::unique_ptr<leveldb::Env> env);

  // Opens leveldb database passing correct options. On success sets |db_| and
  // returns ok status. On failure |db_| is nullptr and returned status reflects
  // failure type.
  leveldb::Status OpenDatabase(const std::string& path, leveldb::Env* env);

  // Destroys leveldb database. Used for recovering after database corruption.
  leveldb::Status DestroyDatabase(const std::string& path, leveldb::Env* env);

  // Attempts to read and return the database's version.
  // If there is not a schema descriptor present, the value returned is 0.
  // If an error occurs, the value returned is kInvalidSchemaVersion(-1).
  int64_t GetStoreVersion();

  // Migrate the db schema from |current_version| to |desired_version|,
  // returning nullopt on success.
  base::Optional<ModelError> Migrate(int64_t current_version,
                                     int64_t desired_version);

  // Migrates from no version record at all (version 0) to version 1 of
  // the schema, returning true on success.
  bool Migrate0To1();

  static void RecordStoreInitResultHistogram(
      StoreInitResultForHistogram result);

  // Helper function for unittests to check if backend already exists for a
  // given path.
  static bool BackendExistsForTest(const std::string& path);

  // In some scenarios ModelTypeStoreBackend holds ownership of env. Typical
  // example is when test creates in memory environment with CreateInMemoryEnv
  // and wants it to be destroyed along with backend. This is achieved by
  // passing ownership of env to TakeEnvOwnership function.
  //
  // env_ declaration should appear before declaration of db_ because
  // environment object should still be valid when db_'s destructor is called.
  std::unique_ptr<leveldb::Env> env_;

  std::unique_ptr<leveldb::DB> db_;

  std::string path_;

  // Ensures that operations with backend are performed seqentially, not
  // concurrently.
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeStoreBackend);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_BACKEND_H_
