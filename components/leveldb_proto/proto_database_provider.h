// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_wrapper.h"

namespace leveldb_proto {

class SharedProtoDatabase;

// A KeyedService that provides instances of ProtoDatabase tied to the current
// profile directory.
class ProtoDatabaseProvider : public KeyedService {
 public:
  using GetSharedDBInstanceCallback =
      base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)>;

  static ProtoDatabaseProvider* Create(const base::FilePath& profile_dir);

  // |client_namespace| is the unique prefix to be used in the shared database
  // if the database returned is a SharedDatabaseClient<T>.
  // |type_prefix| is a unique prefix within the |client_namespace| to be used
  // in the shared database if the database returned is a
  // SharedProtoDatabaseClient<T>.
  // |unique_db_dir|: the subdirectory this database should live in within
  // the profile directory.
  // |task_runner|: the SequencedTaskRunner to run all database operations on.
  // This isn't used by SharedProtoDatabaseClients since all calls using
  // the SharedProtoDatabase will run on its TaskRunner.
  template <typename T>
  std::unique_ptr<ProtoDatabase<T>> GetDB(
      const std::string& client_namespace,
      const std::string& type_prefix,
      const base::FilePath& unique_db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  void GetSharedDBInstance(GetSharedDBInstanceCallback callback);

  ~ProtoDatabaseProvider() override;

 private:
  ProtoDatabaseProvider(const base::FilePath& profile_dir);

  // The shared DB is lazy-initialized, and this ensures that we have a valid
  // instance before triggering the GetSharedDBInstanceCallback on the original
  // calling sequence.
  void PrepareSharedDBInstanceOnTaskRunner();

  void RunGetSharedDBInstanceCallback(GetSharedDBInstanceCallback callback);

  base::FilePath profile_dir_;
  scoped_refptr<SharedProtoDatabase> db_;
  // The SequencedTaskRunner used to ensure thread-safe behaviour for
  // GetSharedDBInstance.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ProtoDatabaseProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseProvider);
};

template <typename T>
std::unique_ptr<ProtoDatabase<T>> ProtoDatabaseProvider::GetDB(
    const std::string& client_namespace,
    const std::string& type_prefix,
    const base::FilePath& unique_db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  return std::make_unique<ProtoDatabaseWrapper<T>>(
      client_namespace, type_prefix, unique_db_dir, task_runner);
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_H_
