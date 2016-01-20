// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"

namespace leveldb_proto {

using KeyValueVector = base::StringPairs;
using KeyVector = std::vector<std::string>;

// When the ProtoDatabaseImpl instance is deleted, in-progress asynchronous
// operations will be completed and the corresponding callbacks will be called.
// Construction/calls/destruction should all happen on the same thread.
template <typename T>
class ProtoDatabaseImpl : public ProtoDatabase<T> {
 public:
  // All blocking calls/disk access will happen on the provided |task_runner|.
  explicit ProtoDatabaseImpl(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ~ProtoDatabaseImpl() override;

  // ProtoDatabase implementation.
  // TODO(cjhopman): Perhaps Init() shouldn't be exposed to users and not just
  //     part of the constructor
  void Init(const char* client_name,
            const base::FilePath& database_dir,
            const typename ProtoDatabase<T>::InitCallback& callback) override;
  void UpdateEntries(
      scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
      scoped_ptr<KeyVector> keys_to_remove,
      const typename ProtoDatabase<T>::UpdateCallback& callback) override;
  void LoadEntries(
      const typename ProtoDatabase<T>::LoadCallback& callback) override;
  void Destroy(
      const typename ProtoDatabase<T>::DestroyCallback& callback) override;

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(
      scoped_ptr<LevelDB> database,
      const base::FilePath& database_dir,
      const typename ProtoDatabase<T>::InitCallback& callback);

 private:
  base::ThreadChecker thread_checker_;

  // Used to run blocking tasks in-order.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<LevelDB> db_;
  base::FilePath database_dir_;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseImpl);
};

namespace {

template <typename T>
void RunInitCallback(const typename ProtoDatabase<T>::InitCallback& callback,
                     const bool* success) {
  callback.Run(*success);
}

template <typename T>
void RunUpdateCallback(
    const typename ProtoDatabase<T>::UpdateCallback& callback,
    const bool* success) {
  callback.Run(*success);
}

template <typename T>
void RunLoadCallback(const typename ProtoDatabase<T>::LoadCallback& callback,
                     const bool* success,
                     scoped_ptr<std::vector<T>> entries) {
  callback.Run(*success, std::move(entries));
}

template <typename T>
void RunDestroyCallback(
    const typename ProtoDatabase<T>::DestroyCallback& callback,
    const bool* success) {
  callback.Run(*success);
}

inline void InitFromTaskRunner(LevelDB* database,
                               const base::FilePath& database_dir,
                               bool* success) {
  DCHECK(success);

  // TODO(cjhopman): Histogram for database size.
  *success = database->Init(database_dir);
}

inline void DestroyFromTaskRunner(const base::FilePath& database_dir,
                                  bool* success) {
  CHECK(success);

  *success = LevelDB::Destroy(database_dir);
}

template <typename T>
void UpdateEntriesFromTaskRunner(
    LevelDB* database,
    scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    scoped_ptr<KeyVector> keys_to_remove,
    bool* success) {
  DCHECK(success);

  // Serialize the values from Proto to string before passing on to database.
  KeyValueVector pairs_to_save;
  for (const auto& pair : *entries_to_save) {
    pairs_to_save.push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  *success = database->Save(pairs_to_save, *keys_to_remove);
}

template <typename T>
void LoadEntriesFromTaskRunner(LevelDB* database,
                               std::vector<T>* entries,
                               bool* success) {
  DCHECK(success);
  DCHECK(entries);

  entries->clear();

  std::vector<std::string> loaded_entries;
  *success = database->Load(&loaded_entries);

  for (const auto& serialized_entry : loaded_entries) {
    T entry;
    if (!entry.ParseFromString(serialized_entry)) {
      DLOG(WARNING) << "Unable to parse leveldb_proto entry";
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }

    entries->push_back(entry);
  }
}

}  // namespace

template <typename T>
ProtoDatabaseImpl<T>::ProtoDatabaseImpl(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {}

template <typename T>
ProtoDatabaseImpl<T>::~ProtoDatabaseImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (db_.get() && !task_runner_->DeleteSoon(FROM_HERE, db_.release()))
    DLOG(WARNING) << "Proto database will not be deleted.";
}

template <typename T>
void ProtoDatabaseImpl<T>::Init(
    const char* client_name,
    const base::FilePath& database_dir,
    const typename ProtoDatabase<T>::InitCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  database_dir_ = database_dir;
  InitWithDatabase(make_scoped_ptr(new LevelDB(client_name)), database_dir,
                   callback);
}

template <typename T>
void ProtoDatabaseImpl<T>::Destroy(
    const typename ProtoDatabase<T>::DestroyCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(db_);
  DCHECK(!database_dir_.empty());

  // Note that |db_| should be released from task runner.
  if (!task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "Proto database will not be deleted.";
    callback.Run(false);
    return;
  }

  // After |db_| is released, we can now wipe out the database directory.
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(DestroyFromTaskRunner, database_dir_, success),
      base::Bind(RunDestroyCallback<T>, callback, base::Owned(success)));
}

template <typename T>
void ProtoDatabaseImpl<T>::InitWithDatabase(
    scoped_ptr<LevelDB> database,
    const base::FilePath& database_dir,
    const typename ProtoDatabase<T>::InitCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!db_);
  DCHECK(database);
  db_.reset(database.release());
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(InitFromTaskRunner, base::Unretained(db_.get()),
                            database_dir, success),
      base::Bind(RunInitCallback<T>, callback, base::Owned(success)));
}

template <typename T>
void ProtoDatabaseImpl<T>::UpdateEntries(
    scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    scoped_ptr<KeyVector> keys_to_remove,
    const typename ProtoDatabase<T>::UpdateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(UpdateEntriesFromTaskRunner<T>, base::Unretained(db_.get()),
                 base::Passed(&entries_to_save), base::Passed(&keys_to_remove),
                 success),
      base::Bind(RunUpdateCallback<T>, callback, base::Owned(success)));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadEntries(
    const typename ProtoDatabase<T>::LoadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool* success = new bool(false);

  scoped_ptr<std::vector<T> > entries(new std::vector<T>());
  // Get this pointer before entries is base::Passed() so we can use it below.
  std::vector<T>* entries_ptr = entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(LoadEntriesFromTaskRunner<T>,
                            base::Unretained(db_.get()), entries_ptr, success),
      base::Bind(RunLoadCallback<T>, callback, base::Owned(success),
                 base::Passed(&entries)));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_
