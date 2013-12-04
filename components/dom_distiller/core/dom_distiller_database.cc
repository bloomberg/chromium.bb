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

DomDistillerDatabase::LevelDB::~LevelDB() {
  DFAKE_SCOPED_LOCK(thread_checker_);
}

bool DomDistillerDatabase::LevelDB::Init(const base::FilePath& database_dir) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.

  std::string path = database_dir.AsUTF8Unsafe();

  leveldb::DB* db = NULL;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (status.IsCorruption()) {
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

bool DomDistillerDatabase::LevelDB::Save(const EntryVector& entries_to_save,
                                         const EntryVector& entries_to_remove) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  leveldb::WriteBatch updates;
  for (EntryVector::const_iterator it = entries_to_save.begin();
       it != entries_to_save.end();
       ++it) {
    updates.Put(leveldb::Slice(it->entry_id()),
                leveldb::Slice(it->SerializeAsString()));
  }
  for (EntryVector::const_iterator it = entries_to_remove.begin();
       it != entries_to_remove.end();
       ++it) {
    updates.Delete(leveldb::Slice(it->entry_id()));
  }

  leveldb::WriteOptions options;
  options.sync = true;
  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok())
    return true;

  DLOG(WARNING) << "Failed writing dom_distiller entries: "
                << status.ToString();
  return false;
}

bool DomDistillerDatabase::LevelDB::Load(EntryVector* entries) {
  DFAKE_SCOPED_LOCK(thread_checker_);

  leveldb::ReadOptions options;
  scoped_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  for (db_iterator->SeekToFirst(); db_iterator->Valid(); db_iterator->Next()) {
    leveldb::Slice value_slice = db_iterator->value();

    ArticleEntry entry;
    if (!entry.ParseFromArray(value_slice.data(), value_slice.size())) {
      DLOG(WARNING) << "Unable to parse dom_distiller entry "
                    << db_iterator->key().ToString();
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }
    entries->push_back(entry);
  }
  return true;
}

namespace {

void RunInitCallback(DomDistillerDatabaseInterface::InitCallback callback,
                     const bool* success) {
  callback.Run(*success);
}

void RunUpdateCallback(DomDistillerDatabaseInterface::UpdateCallback callback,
                     const bool* success) {
  callback.Run(*success);
}

void RunLoadCallback(DomDistillerDatabaseInterface::LoadCallback callback,
                     const bool* success,
                     scoped_ptr<EntryVector> entries) {
  callback.Run(*success, entries.Pass());
}

void InitFromTaskRunner(DomDistillerDatabase::Database* database,
                        const base::FilePath& database_dir,
                        bool* success) {
  DCHECK(success);

  // TODO(cjhopman): Histogram for database size.
  *success = database->Init(database_dir);
}

void UpdateEntriesFromTaskRunner(DomDistillerDatabase::Database* database,
                               scoped_ptr<EntryVector> entries_to_save,
                               scoped_ptr<EntryVector> entries_to_remove,
                               bool* success) {
  DCHECK(success);
  *success = database->Save(*entries_to_save, *entries_to_remove);
}

void LoadEntriesFromTaskRunner(DomDistillerDatabase::Database* database,
                               EntryVector* entries,
                               bool* success) {
  DCHECK(success);
  DCHECK(entries);

  entries->clear();
  *success = database->Load(entries);
}

}  // namespace

DomDistillerDatabase::DomDistillerDatabase(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

void DomDistillerDatabase::Init(const base::FilePath& database_dir,
                                InitCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InitWithDatabase(scoped_ptr<Database>(new LevelDB()), database_dir, callback);
}

void DomDistillerDatabase::InitWithDatabase(scoped_ptr<Database> database,
                                            const base::FilePath& database_dir,
                                            InitCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!db_);
  DCHECK(database);
  db_.reset(database.release());
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(InitFromTaskRunner,
                 base::Unretained(db_.get()),
                 database_dir,
                 success),
      base::Bind(RunInitCallback, callback, base::Owned(success)));
}

void DomDistillerDatabase::UpdateEntries(
    scoped_ptr<EntryVector> entries_to_save,
    scoped_ptr<EntryVector> entries_to_remove,
    UpdateCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(UpdateEntriesFromTaskRunner,
                 base::Unretained(db_.get()),
                 base::Passed(&entries_to_save),
                 base::Passed(&entries_to_remove),
                 success),
      base::Bind(RunUpdateCallback, callback, base::Owned(success)));
}

void DomDistillerDatabase::LoadEntries(LoadCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool* success = new bool(false);

  scoped_ptr<EntryVector> entries(new EntryVector());
  // Get this pointer before entries is base::Passed() so we can use it below.
  EntryVector* entries_ptr = entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(LoadEntriesFromTaskRunner,
                 base::Unretained(db_.get()),
                 entries_ptr,
                 success),
      base::Bind(RunLoadCallback,
                 callback,
                 base::Owned(success),
                 base::Passed(&entries)));
}

DomDistillerDatabase::~DomDistillerDatabase() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "DOM distiller database will not be deleted.";
  }
}

}  // namespace dom_distiller
