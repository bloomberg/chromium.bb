// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_PROVIDER_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/internal/proto_database_impl.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {

class SharedProtoDatabase;

// A KeyedService that provides instances of ProtoDatabase tied to the current
// profile directory.
class ProtoDatabaseProvider : public KeyedService {
 public:
  using GetSharedDBInstanceCallback =
      base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)>;

  static ProtoDatabaseProvider* Create(const base::FilePath& profile_dir);

  template <typename T>
  static std::unique_ptr<ProtoDatabase<T>> CreateUniqueDB(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
    return std::make_unique<ProtoDatabaseImpl<T>>(task_runner);
  }

  // |db_type|: Each database should have a type specified in ProtoDbType enum.
  // This type is used to index data in the shared database. |unique_db_dir|:
  // the subdirectory this database should live in within the profile directory.
  // |task_runner|: the SequencedTaskRunner to run all database operations on.
  // This isn't used by SharedProtoDatabaseClients since all calls using
  // the SharedProtoDatabase will run on its TaskRunner.
  template <typename T>
  std::unique_ptr<ProtoDatabase<T>> GetDB(
      ProtoDbType db_type,
      const base::FilePath& unique_db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  virtual void GetSharedDBInstance(
      GetSharedDBInstanceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  ~ProtoDatabaseProvider() override;

 private:
  friend class TestProtoDatabaseProvider;
  friend class ProtoDatabaseImplTest;

  ProtoDatabaseProvider(const base::FilePath& profile_dir);

  base::FilePath profile_dir_;
  scoped_refptr<SharedProtoDatabase> db_;
  base::Lock get_db_lock_;
  // The SequencedTaskRunner used to ensure thread-safe behaviour for
  // GetSharedDBInstance when called from multiple clients.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // We store the creation sequence because we want to use that to make requests
  // to the main provider that rely on WeakPtrs from this, so they're all
  // invalidated/checked on the same sequence.
  scoped_refptr<base::SequencedTaskRunner> creation_sequence_;

  base::WeakPtrFactory<ProtoDatabaseProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseProvider);
};

template <typename T>
std::unique_ptr<ProtoDatabase<T>> ProtoDatabaseProvider::GetDB(
    ProtoDbType db_type,
    const base::FilePath& unique_db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  return base::WrapUnique(new ProtoDatabaseImpl<T>(
      db_type, unique_db_dir, task_runner,
      base::WrapUnique(new SharedProtoDatabaseProvider(
          creation_sequence_, weak_factory_.GetWeakPtr()))));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_PROTO_DATABASE_PROVIDER_H_
