// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_database.h"

#include <set>

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using content::BrowserThread;

namespace contacts {

namespace {

// Initialization results reported via the "Contacts.DatabaseInitResult"
// histogram.
enum HistogramInitResult {
  HISTOGRAM_INIT_RESULT_SUCCESS = 0,
  HISTOGRAM_INIT_RESULT_FAILURE = 1,
  HISTOGRAM_INIT_RESULT_DELETED_CORRUPTED = 2,
  HISTOGRAM_INIT_RESULT_MAX_VALUE = 3,
};

// Save results reported via the "Contacts.DatabaseSaveResult" histogram.
enum HistogramSaveResult {
  HISTOGRAM_SAVE_RESULT_SUCCESS = 0,
  HISTOGRAM_SAVE_RESULT_FAILURE = 1,
  HISTOGRAM_SAVE_RESULT_MAX_VALUE = 2,
};

// Load results reported via the "Contacts.DatabaseLoadResult" histogram.
enum HistogramLoadResult {
  HISTOGRAM_LOAD_RESULT_SUCCESS = 0,
  HISTOGRAM_LOAD_RESULT_METADATA_PARSE_FAILURE = 1,
  HISTOGRAM_LOAD_RESULT_CONTACT_PARSE_FAILURE = 2,
  HISTOGRAM_LOAD_RESULT_MAX_VALUE = 3,
};

// LevelDB key used for storing UpdateMetadata messages.
const char kUpdateMetadataKey[] = "__chrome_update_metadata__";

}  // namespace

ContactDatabase::ContactDatabase()
    : weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  task_runner_ = pool->GetSequencedTaskRunner(pool->GetSequenceToken());
}

void ContactDatabase::DestroyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  weak_ptr_factory_.InvalidateWeakPtrs();
  task_runner_->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&ContactDatabase::DestroyFromTaskRunner,
                 base::Unretained(this)));
}

void ContactDatabase::Init(const base::FilePath& database_dir,
                           InitCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ContactDatabase::InitFromTaskRunner,
                 base::Unretained(this),
                 database_dir,
                 success),
      base::Bind(&ContactDatabase::RunInitCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(success)));
}

void ContactDatabase::SaveContacts(scoped_ptr<ContactPointers> contacts_to_save,
                                   scoped_ptr<ContactIds> contact_ids_to_delete,
                                   scoped_ptr<UpdateMetadata> metadata,
                                   bool is_full_update,
                                   SaveCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ContactDatabase::SaveContactsFromTaskRunner,
                 base::Unretained(this),
                 base::Passed(&contacts_to_save),
                 base::Passed(&contact_ids_to_delete),
                 base::Passed(&metadata),
                 is_full_update,
                 success),
      base::Bind(&ContactDatabase::RunSaveCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(success)));
}

void ContactDatabase::LoadContacts(LoadCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool* success = new bool(false);
  scoped_ptr<ScopedVector<Contact> > contacts(new ScopedVector<Contact>);
  scoped_ptr<UpdateMetadata> metadata(new UpdateMetadata);

  // Extract pointers before we calling Pass() so we can use them below.
  ScopedVector<Contact>* contacts_ptr = contacts.get();
  UpdateMetadata* metadata_ptr = metadata.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ContactDatabase::LoadContactsFromTaskRunner,
                 base::Unretained(this),
                 success,
                 contacts_ptr,
                 metadata_ptr),
      base::Bind(&ContactDatabase::RunLoadCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(success),
                 base::Passed(&contacts),
                 base::Passed(&metadata)));
}

ContactDatabase::~ContactDatabase() {
  DCHECK(IsRunByTaskRunner());
}

bool ContactDatabase::IsRunByTaskRunner() const {
  return BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread();
}

void ContactDatabase::DestroyFromTaskRunner() {
  DCHECK(IsRunByTaskRunner());
  delete this;
}

void ContactDatabase::RunInitCallback(InitCallback callback,
                                      const bool* success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(*success);
}

void ContactDatabase::RunSaveCallback(SaveCallback callback,
                                      const bool* success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(*success);
}

void ContactDatabase::RunLoadCallback(
    LoadCallback callback,
    const bool* success,
    scoped_ptr<ScopedVector<Contact> > contacts,
    scoped_ptr<UpdateMetadata> metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(*success, contacts.Pass(), metadata.Pass());
}

void ContactDatabase::InitFromTaskRunner(const base::FilePath& database_dir,
                                         bool* success) {
  DCHECK(IsRunByTaskRunner());
  DCHECK(success);

  VLOG(1) << "Opening " << database_dir.value();
  UMA_HISTOGRAM_MEMORY_KB("Contacts.DatabaseSizeBytes",
                          file_util::ComputeDirectorySize(database_dir));
  *success = false;
  HistogramInitResult histogram_result = HISTOGRAM_INIT_RESULT_SUCCESS;

  leveldb::Options options;
  options.create_if_missing = true;
  bool delete_and_retry_on_corruption = true;

  while (true) {
    leveldb::DB* db = NULL;
    leveldb::Status status =
        leveldb::DB::Open(options, database_dir.value(), &db);
    if (status.ok()) {
      CHECK(db);
      db_.reset(db);
      *success = true;
      return;
    }

    LOG(WARNING) << "Unable to open " << database_dir.value() << ": "
                 << status.ToString();

    // Delete the existing database and try again (just once, though).
    if (status.IsCorruption() && delete_and_retry_on_corruption) {
      LOG(WARNING) << "Deleting possibly-corrupt database";
      file_util::Delete(database_dir, true);
      delete_and_retry_on_corruption = false;
      histogram_result = HISTOGRAM_INIT_RESULT_DELETED_CORRUPTED;
    } else {
      histogram_result = HISTOGRAM_INIT_RESULT_FAILURE;
      break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Contacts.DatabaseInitResult",
                            histogram_result,
                            HISTOGRAM_INIT_RESULT_MAX_VALUE);
}

void ContactDatabase::SaveContactsFromTaskRunner(
    scoped_ptr<ContactPointers> contacts_to_save,
    scoped_ptr<ContactIds> contact_ids_to_delete,
    scoped_ptr<UpdateMetadata> metadata,
    bool is_full_update,
    bool* success) {
  DCHECK(IsRunByTaskRunner());
  DCHECK(success);
  VLOG(1) << "Saving " << contacts_to_save->size() << " contact(s) to database "
          << "and deleting " << contact_ids_to_delete->size() << " as "
          << (is_full_update ? "full" : "incremental") << " update";

  *success = false;

  // If we're doing a full update, find all of the existing keys first so we can
  // delete ones that aren't present in the new set of contacts.
  std::set<std::string> keys_to_delete;
  if (is_full_update) {
    leveldb::ReadOptions options;
    scoped_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
    db_iterator->SeekToFirst();
    while (db_iterator->Valid()) {
      std::string key = db_iterator->key().ToString();
      if (key != kUpdateMetadataKey)
        keys_to_delete.insert(key);
      db_iterator->Next();
    }
  } else {
    for (ContactIds::const_iterator it = contact_ids_to_delete->begin();
         it != contact_ids_to_delete->end(); ++it) {
      keys_to_delete.insert(*it);
    }
  }

  // TODO(derat): Serializing all of the contacts and so we can write them in a
  // single batch may be expensive, memory-wise.  Consider writing them in
  // several batches instead.  (To avoid using partial writes in the event of a
  // crash, maybe add a dummy "write completed" contact that's removed in the
  // first batch and added in the last.)
  leveldb::WriteBatch updates;
  for (ContactPointers::const_iterator it = contacts_to_save->begin();
       it != contacts_to_save->end(); ++it) {
    const contacts::Contact& contact = **it;
    if (contact.contact_id() == kUpdateMetadataKey) {
      LOG(WARNING) << "Skipping contact with reserved ID "
                   << contact.contact_id();
      continue;
    }
    updates.Put(leveldb::Slice(contact.contact_id()),
                leveldb::Slice(contact.SerializeAsString()));
    if (is_full_update)
      keys_to_delete.erase(contact.contact_id());
  }

  for (std::set<std::string>::const_iterator it = keys_to_delete.begin();
       it != keys_to_delete.end(); ++it) {
    updates.Delete(leveldb::Slice(*it));
  }

  updates.Put(leveldb::Slice(kUpdateMetadataKey),
              leveldb::Slice(metadata->SerializeAsString()));

  leveldb::WriteOptions options;
  options.sync = true;
  leveldb::Status status = db_->Write(options, &updates);
  if (status.ok())
    *success = true;
  else
    LOG(WARNING) << "Failed writing contacts: " << status.ToString();

  UMA_HISTOGRAM_ENUMERATION("Contacts.DatabaseSaveResult",
                            *success ?
                            HISTOGRAM_SAVE_RESULT_SUCCESS :
                            HISTOGRAM_SAVE_RESULT_FAILURE,
                            HISTOGRAM_SAVE_RESULT_MAX_VALUE);
}

void ContactDatabase::LoadContactsFromTaskRunner(
    bool* success,
    ScopedVector<Contact>* contacts,
    UpdateMetadata* metadata) {
  DCHECK(IsRunByTaskRunner());
  DCHECK(success);
  DCHECK(contacts);
  DCHECK(metadata);

  *success = false;
  contacts->clear();
  metadata->Clear();

  leveldb::ReadOptions options;
  scoped_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  db_iterator->SeekToFirst();
  while (db_iterator->Valid()) {
    leveldb::Slice value_slice = db_iterator->value();

    if (db_iterator->key().ToString() == kUpdateMetadataKey) {
      if (!metadata->ParseFromArray(value_slice.data(), value_slice.size())) {
        LOG(WARNING) << "Unable to parse metadata";
        UMA_HISTOGRAM_ENUMERATION("Contacts.DatabaseLoadResult",
                                  HISTOGRAM_LOAD_RESULT_METADATA_PARSE_FAILURE,
                                  HISTOGRAM_LOAD_RESULT_MAX_VALUE);
        return;
      }
    } else {
      scoped_ptr<Contact> contact(new Contact);
      if (!contact->ParseFromArray(value_slice.data(), value_slice.size())) {
        LOG(WARNING) << "Unable to parse contact "
                     << db_iterator->key().ToString();
        UMA_HISTOGRAM_ENUMERATION("Contacts.DatabaseLoadResult",
                                  HISTOGRAM_LOAD_RESULT_CONTACT_PARSE_FAILURE,
                                  HISTOGRAM_LOAD_RESULT_MAX_VALUE);
        return;
      }
      contacts->push_back(contact.release());
    }
    db_iterator->Next();
  }

  *success = true;
  UMA_HISTOGRAM_ENUMERATION("Contacts.DatabaseLoadResult",
                            HISTOGRAM_LOAD_RESULT_SUCCESS,
                            HISTOGRAM_LOAD_RESULT_MAX_VALUE);
}

}  // namespace contacts
