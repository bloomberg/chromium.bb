// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_database.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/dom_distiller/core/article_entry.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using base::MessageLoop;
using base::SequencedTaskRunner;

namespace dom_distiller {

DomDistillerDatabase::LevelDB::LevelDB() {}

DomDistillerDatabase::LevelDB::~LevelDB() {}

bool DomDistillerDatabase::LevelDB::Init(const base::FilePath& database_dir) {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.

  std::string path = database_dir.AsUTF8Unsafe();

  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (status.IsCorruption()) {
    LOG(WARNING) << "Deleting possibly-corrupt database";
    base::DeleteFile(database_dir, true);
    status = leveldb::DB::Open(options, path, &db);
  }

  if (status.ok()) {
    CHECK(db);
    db_.reset(db);
    return true;
  }

  LOG(WARNING) << "Unable to open " << database_dir.value() << ": "
               << status.ToString();
  return false;
}

bool DomDistillerDatabase::LevelDB::Save(const EntryVector& entries) {
  leveldb::WriteBatch updates;
  for (EntryVector::const_iterator it = entries.begin(); it != entries.end();
       ++it) {
    updates.Put(leveldb::Slice(it->entry_id()),
                leveldb::Slice(it->SerializeAsString()));
  }

  leveldb::WriteOptions options;
  options.sync = true;
  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok())
    return true;

  LOG(WARNING) << "Failed writing dom_distiller entries: " << status.ToString();
  return false;
}

bool DomDistillerDatabase::LevelDB::Load(EntryVector* entries) {
  leveldb::ReadOptions options;
  scoped_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice value_slice = db_iterator->value();

    ArticleEntry entry;
    if (!entry.ParseFromArray(value_slice.data(), value_slice.size())) {
      LOG(WARNING) << "Unable to parse dom_distiller entry "
                   << db_iterator->key().ToString();
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }
    entries->push_back(entry);
  }
  return true;
}

DomDistillerDatabase::DomDistillerDatabase(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  main_loop_ = MessageLoop::current();
}

void DomDistillerDatabase::Destroy() {
  DCHECK(IsRunOnMainLoop());
  weak_ptr_factory_.InvalidateWeakPtrs();
  task_runner_->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&DomDistillerDatabase::DestroyFromTaskRunner,
                 base::Unretained(this)));
}

void DomDistillerDatabase::Init(const base::FilePath& database_dir,
                                InitCallback callback) {
  InitWithDatabase(scoped_ptr<Database>(new LevelDB()), database_dir, callback);
}

namespace {

void RunInitCallback(DomDistillerDatabaseInterface::InitCallback callback,
                     const bool* success) {
  callback.Run(*success);
}

void RunSaveCallback(DomDistillerDatabaseInterface::SaveCallback callback,
                     const bool* success) {
  callback.Run(*success);
}

void RunLoadCallback(DomDistillerDatabaseInterface::LoadCallback callback,
                     const bool* success,
                     scoped_ptr<EntryVector> entries) {
  callback.Run(*success, entries.Pass());
}

}  // namespace

void DomDistillerDatabase::InitWithDatabase(scoped_ptr<Database> database,
                                            const base::FilePath& database_dir,
                                            InitCallback callback) {
  DCHECK(IsRunOnMainLoop());
  DCHECK(!db_);
  DCHECK(database);
  db_.reset(database.release());
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DomDistillerDatabase::InitFromTaskRunner,
                 base::Unretained(this),
                 database_dir,
                 success),
      base::Bind(RunInitCallback, callback, base::Owned(success)));
}

void DomDistillerDatabase::SaveEntries(scoped_ptr<EntryVector> entries,
                                       SaveCallback callback) {
  DCHECK(IsRunOnMainLoop());
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DomDistillerDatabase::SaveEntriesFromTaskRunner,
                 base::Unretained(this),
                 base::Passed(&entries),
                 success),
      base::Bind(RunSaveCallback, callback, base::Owned(success)));
}

void DomDistillerDatabase::LoadEntries(LoadCallback callback) {
  DCHECK(IsRunOnMainLoop());

  bool* success = new bool(false);

  scoped_ptr<EntryVector> entries(new EntryVector());
  // Get this pointer before entries is base::Passed() so we can use it below.
  EntryVector* entries_ptr = entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DomDistillerDatabase::LoadEntriesFromTaskRunner,
                 base::Unretained(this),
                 entries_ptr,
                 success),
      base::Bind(RunLoadCallback,
                 callback,
                 base::Owned(success),
                 base::Passed(&entries)));
}

DomDistillerDatabase::~DomDistillerDatabase() { DCHECK(IsRunByTaskRunner()); }

bool DomDistillerDatabase::IsRunByTaskRunner() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool DomDistillerDatabase::IsRunOnMainLoop() const {
  return MessageLoop::current() == main_loop_;
}

void DomDistillerDatabase::DestroyFromTaskRunner() {
  DCHECK(IsRunByTaskRunner());
  delete this;
}

void DomDistillerDatabase::InitFromTaskRunner(
    const base::FilePath& database_dir,
    bool* success) {
  DCHECK(IsRunByTaskRunner());
  DCHECK(success);

  VLOG(1) << "Opening " << database_dir.value();

  // TODO(cjhopman): Histogram for database size.
  *success = db_->Init(database_dir);
}

void DomDistillerDatabase::SaveEntriesFromTaskRunner(
    scoped_ptr<EntryVector> entries,
    bool* success) {
  DCHECK(IsRunByTaskRunner());
  DCHECK(success);
  VLOG(1) << "Saving " << entries->size() << " entry(ies) to database ";
  *success = db_->Save(*entries);
}

void DomDistillerDatabase::LoadEntriesFromTaskRunner(EntryVector* entries,
                                                     bool* success) {
  DCHECK(IsRunByTaskRunner());
  DCHECK(success);
  DCHECK(entries);

  entries->clear();
  *success = db_->Load(entries);
}

}  // namespace dom_distiller
