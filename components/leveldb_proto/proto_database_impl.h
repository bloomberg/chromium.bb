// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"

namespace leveldb_proto {

typedef std::vector<std::pair<std::string, std::string> > KeyValueVector;
typedef std::vector<std::string> KeyVector;

// When the ProtoDatabaseImpl instance is deleted, in-progress asynchronous
// operations will be completed and the corresponding callbacks will be called.
// Construction/calls/destruction should all happen on the same thread.
template <typename T>
class ProtoDatabaseImpl : public ProtoDatabase<T> {
 public:
  // All blocking calls/disk access will happen on the provided |task_runner|.
  explicit ProtoDatabaseImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~ProtoDatabaseImpl();

  // ProtoDatabase implementation.
  // TODO(cjhopman): Perhaps Init() shouldn't be exposed to users and not just
  //     part of the constructor
  virtual void Init(const base::FilePath& database_dir,
                    typename ProtoDatabase<T>::InitCallback callback) OVERRIDE;
  virtual void UpdateEntries(
      scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
      scoped_ptr<KeyVector> keys_to_remove,
      typename ProtoDatabase<T>::UpdateCallback callback) OVERRIDE;
  virtual void LoadEntries(
      typename ProtoDatabase<T>::LoadCallback callback) OVERRIDE;

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(scoped_ptr<LevelDB> database,
                        const base::FilePath& database_dir,
                        typename ProtoDatabase<T>::InitCallback callback);

 private:
  base::ThreadChecker thread_checker_;

  // Used to run blocking tasks in-order.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<LevelDB> db_;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseImpl);
};

namespace {

template <typename T>
void RunInitCallback(typename ProtoDatabase<T>::InitCallback callback,
                     const bool* success) {
  callback.Run(*success);
}

template <typename T>
void RunUpdateCallback(typename ProtoDatabase<T>::UpdateCallback callback,
                       const bool* success) {
  callback.Run(*success);
}

template <typename T>
void RunLoadCallback(typename ProtoDatabase<T>::LoadCallback callback,
                     const bool* success, scoped_ptr<std::vector<T> > entries) {
  callback.Run(*success, entries.Pass());
}

void InitFromTaskRunner(LevelDB* database, const base::FilePath& database_dir,
                        bool* success) {
  DCHECK(success);

  // TODO(cjhopman): Histogram for database size.
  *success = database->Init(database_dir);
}

template <typename T>
void UpdateEntriesFromTaskRunner(
    LevelDB* database,
    scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    scoped_ptr<KeyVector> keys_to_remove, bool* success) {
  DCHECK(success);
  // Serialize the values from Proto to string before passing on to database.
  KeyValueVector pairs_to_save;
  for (typename ProtoDatabase<T>::KeyEntryVector::iterator it =
           entries_to_save->begin();
       it != entries_to_save->end(); ++it) {
    pairs_to_save.push_back(
        std::make_pair(it->first, it->second.SerializeAsString()));
  }
  *success = database->Save(pairs_to_save, *keys_to_remove);
}

template <typename T>
void LoadEntriesFromTaskRunner(LevelDB* database, std::vector<T>* entries,
                               bool* success) {
  DCHECK(success);
  DCHECK(entries);

  entries->clear();
  std::vector<std::string> loaded_entries;
  *success = database->Load(&loaded_entries);
  for (std::vector<std::string>::iterator it = loaded_entries.begin();
       it != loaded_entries.end(); ++it) {
    T entry;
    if (!entry.ParseFromString(*it)) {
      DLOG(WARNING) << "Unable to parse leveldb_proto entry " << *it;
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }
    entries->push_back(entry);
  }
}

}  // namespace

template <typename T>
ProtoDatabaseImpl<T>::ProtoDatabaseImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}

template <typename T>
ProtoDatabaseImpl<T>::~ProtoDatabaseImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "DOM distiller database will not be deleted.";
  }
}

template <typename T>
void ProtoDatabaseImpl<T>::Init(
    const base::FilePath& database_dir,
    typename ProtoDatabase<T>::InitCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InitWithDatabase(scoped_ptr<LevelDB>(new LevelDB()), database_dir, callback);
}

template <typename T>
void ProtoDatabaseImpl<T>::InitWithDatabase(
    scoped_ptr<LevelDB> database, const base::FilePath& database_dir,
    typename ProtoDatabase<T>::InitCallback callback) {
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
    typename ProtoDatabase<T>::UpdateCallback callback) {
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
    typename ProtoDatabase<T>::LoadCallback callback) {
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
