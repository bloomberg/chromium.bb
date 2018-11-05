// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_

#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

// An implementation of ProtoDatabase<T> that manages the lifecycle of a unique
// LevelDB instance.
template <typename T>
class UniqueProtoDatabase : public ProtoDatabase<T> {
 public:
  UniqueProtoDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  UniqueProtoDatabase(
      const base::FilePath& database_dir,
      const leveldb_env::Options& options,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // This version of Init is for compatibility, since many of the current
  // proto database still use this.
  virtual void Init(const char* client_name,
                    const base::FilePath& database_dir,
                    const leveldb_env::Options& options,
                    typename ProtoDatabase<T>::InitCallback callback) override;

  virtual void Init(const std::string& client_name,
                    typename ProtoDatabase<T>::InitCallback callback) override;

  virtual ~UniqueProtoDatabase();

 private:
  THREAD_CHECKER(thread_checker_);

  base::FilePath database_dir_;
  leveldb_env::Options options_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<LevelDB> db_;
};

template <typename T>
UniqueProtoDatabase<T>::UniqueProtoDatabase(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : ProtoDatabase<T>(task_runner) {}

template <typename T>
UniqueProtoDatabase<T>::UniqueProtoDatabase(
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : ProtoDatabase<T>(task_runner) {
  database_dir_ = database_dir;
  options_ = options;
}

template <typename T>
UniqueProtoDatabase<T>::~UniqueProtoDatabase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (db_.get() &&
      !this->db_wrapper_->task_runner()->DeleteSoon(FROM_HERE, db_.release()))
    DLOG(WARNING) << "Proto database will not be deleted.";
}

template <typename T>
void UniqueProtoDatabase<T>::Init(
    const std::string& client_name,
    typename ProtoDatabase<T>::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  db_ = std::make_unique<LevelDB>(client_name.c_str());
  ProtoDatabase<T>::InitWithDatabase(db_.get(), database_dir_, options_,
                                     std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::Init(
    const char* client_name,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    typename ProtoDatabase<T>::InitCallback callback) {
  database_dir_ = database_dir;
  options_ = options;
  Init(std::string(client_name), std::move(callback));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_
