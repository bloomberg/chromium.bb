// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_H_

#include "base/files/file_path.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_wrapper.h"

namespace leveldb_proto {

// A KeyedService that provides instances of ProtoDatabase tied to the current
// profile directory.
class ProtoDatabaseProvider : public KeyedService {
 public:
  static ProtoDatabaseProvider* Create(const base::FilePath& profile_dir);

  // |unique_db_dir|: the subdirectory this database should live in within
  // the profile directory.
  // |task_runner|: the SequencedTaskRunner to run all database operations on.
  template <typename T>
  std::unique_ptr<ProtoDatabase<T>> GetDB(
      const base::FilePath& unique_db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ~ProtoDatabaseProvider() override = default;

 private:
  ProtoDatabaseProvider(const base::FilePath& profile_dir);

  base::FilePath profile_dir_;
};

template <typename T>
std::unique_ptr<ProtoDatabase<T>> ProtoDatabaseProvider::GetDB(
    const base::FilePath& unique_db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  return std::make_unique<ProtoDatabaseWrapper<T>>(unique_db_dir, task_runner);
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_H_