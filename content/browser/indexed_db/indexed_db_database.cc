// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <math.h>
#include <vector>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_index_writer.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"

using base::Int64ToString16;
using WebKit::WebIDBKey;

namespace content {

class CreateObjectStoreOperation : public IndexedDBTransaction::Operation {
 public:
  CreateObjectStoreOperation(
      scoped_refptr<IndexedDBBackingStore> backing_store,
      const IndexedDBObjectStoreMetadata& object_store_metadata)
      : backing_store_(backing_store),
        object_store_metadata_(object_store_metadata) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const IndexedDBObjectStoreMetadata object_store_metadata_;
};

class DeleteObjectStoreOperation : public IndexedDBTransaction::Operation {
 public:
  DeleteObjectStoreOperation(
      scoped_refptr<IndexedDBBackingStore> backing_store,
      const IndexedDBObjectStoreMetadata& object_store_metadata)
      : backing_store_(backing_store),
        object_store_metadata_(object_store_metadata) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const IndexedDBObjectStoreMetadata object_store_metadata_;
};

class IndexedDBDatabase::VersionChangeOperation
    : public IndexedDBTransaction::Operation {
 public:
  VersionChangeOperation(
      scoped_refptr<IndexedDBDatabase> database,
      int64 transaction_id,
      int64 version,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks)
      : database_(database),
        transaction_id_(transaction_id),
        version_(version),
        callbacks_(callbacks),
        database_callbacks_(database_callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBDatabase> database_;
  int64 transaction_id_;
  int64 version_;
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
  scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks_;
};

class CreateObjectStoreAbortOperation : public IndexedDBTransaction::Operation {
 public:
  CreateObjectStoreAbortOperation(scoped_refptr<IndexedDBDatabase> database,
                                  int64 object_store_id)
      : database_(database), object_store_id_(object_store_id) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBDatabase> database_;
  const int64 object_store_id_;
};

class DeleteObjectStoreAbortOperation : public IndexedDBTransaction::Operation {
 public:
  DeleteObjectStoreAbortOperation(
      scoped_refptr<IndexedDBDatabase> database,
      const IndexedDBObjectStoreMetadata& object_store_metadata)
      : database_(database), object_store_metadata_(object_store_metadata) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBDatabase> database_;
  IndexedDBObjectStoreMetadata object_store_metadata_;
};

class IndexedDBDatabase::VersionChangeAbortOperation
    : public IndexedDBTransaction::Operation {
 public:
  VersionChangeAbortOperation(scoped_refptr<IndexedDBDatabase> database,
                              const string16& previous_version,
                              int64 previous_int_version)
      : database_(database),
        previous_version_(previous_version),
        previous_int_version_(previous_int_version) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBDatabase> database_;
  string16 previous_version_;
  int64 previous_int_version_;
};

class CreateIndexOperation : public IndexedDBTransaction::Operation {
 public:
  CreateIndexOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
                       int64 object_store_id,
                       const IndexedDBIndexMetadata& index_metadata)
      : backing_store_(backing_store),
        object_store_id_(object_store_id),
        index_metadata_(index_metadata) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 object_store_id_;
  const IndexedDBIndexMetadata index_metadata_;
};

class DeleteIndexOperation : public IndexedDBTransaction::Operation {
 public:
  DeleteIndexOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
                       int64 object_store_id,
                       const IndexedDBIndexMetadata& index_metadata)
      : backing_store_(backing_store),
        object_store_id_(object_store_id),
        index_metadata_(index_metadata) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 object_store_id_;
  const IndexedDBIndexMetadata index_metadata_;
};

class CreateIndexAbortOperation : public IndexedDBTransaction::Operation {
 public:
  CreateIndexAbortOperation(scoped_refptr<IndexedDBDatabase> database,
                            int64 object_store_id,
                            int64 index_id)
      : database_(database),
        object_store_id_(object_store_id),
        index_id_(index_id) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBDatabase> database_;
  const int64 object_store_id_;
  const int64 index_id_;
};

class DeleteIndexAbortOperation : public IndexedDBTransaction::Operation {
 public:
  DeleteIndexAbortOperation(scoped_refptr<IndexedDBDatabase> database,
                            int64 object_store_id,
                            const IndexedDBIndexMetadata& index_metadata)
      : database_(database),
        object_store_id_(object_store_id),
        index_metadata_(index_metadata) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBDatabase> database_;
  const int64 object_store_id_;
  const IndexedDBIndexMetadata index_metadata_;
};

class GetOperation : public IndexedDBTransaction::Operation {
 public:
  GetOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
               int64 database_id,
               int64 object_store_id,
               int64 index_id,
               const IndexedDBKeyPath& key_path,
               const bool auto_increment,
               scoped_ptr<IndexedDBKeyRange> key_range,
               indexed_db::CursorType cursor_type,
               scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : backing_store_(backing_store),
        database_id_(database_id),
        object_store_id_(object_store_id),
        index_id_(index_id),
        key_path_(key_path),
        auto_increment_(auto_increment),
        key_range_(key_range.Pass()),
        cursor_type_(cursor_type),
        callbacks_(callbacks) {
  }
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 database_id_;
  const int64 object_store_id_;
  const int64 index_id_;
  const IndexedDBKeyPath key_path_;
  const bool auto_increment_;
  const scoped_ptr<IndexedDBKeyRange> key_range_;
  const indexed_db::CursorType cursor_type_;
  const scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class PutOperation : public IndexedDBTransaction::Operation {
 public:
  PutOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
               int64 database_id,
               const IndexedDBObjectStoreMetadata& object_store,
               std::vector<char>* value,
               scoped_ptr<IndexedDBKey> key,
               IndexedDBDatabase::PutMode put_mode,
               scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
               const std::vector<int64>& index_ids,
               const std::vector<IndexedDBDatabase::IndexKeys>& index_keys)
      : backing_store_(backing_store),
        database_id_(database_id),
        object_store_(object_store),
        key_(key.Pass()),
        put_mode_(put_mode),
        callbacks_(callbacks),
        index_ids_(index_ids),
        index_keys_(index_keys) {
    value_.swap(*value);
  }
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 database_id_;
  const IndexedDBObjectStoreMetadata object_store_;
  std::vector<char> value_;
  scoped_ptr<IndexedDBKey> key_;
  const IndexedDBDatabase::PutMode put_mode_;
  const scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
  const std::vector<int64> index_ids_;
  const std::vector<IndexedDBDatabase::IndexKeys> index_keys_;
};

class SetIndexesReadyOperation : public IndexedDBTransaction::Operation {
 public:
  explicit SetIndexesReadyOperation(size_t index_count)
      : index_count_(index_count) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const size_t index_count_;
};

class OpenCursorOperation : public IndexedDBTransaction::Operation {
 public:
  OpenCursorOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
                      int64 database_id,
                      int64 object_store_id,
                      int64 index_id,
                      scoped_ptr<IndexedDBKeyRange> key_range,
                      indexed_db::CursorDirection direction,
                      indexed_db::CursorType cursor_type,
                      IndexedDBDatabase::TaskType task_type,
                      scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : backing_store_(backing_store),
        database_id_(database_id),
        object_store_id_(object_store_id),
        index_id_(index_id),
        key_range_(key_range.Pass()),
        direction_(direction),
        cursor_type_(cursor_type),
        task_type_(task_type),
        callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 database_id_;
  const int64 object_store_id_;
  const int64 index_id_;
  const scoped_ptr<IndexedDBKeyRange> key_range_;
  const indexed_db::CursorDirection direction_;
  const indexed_db::CursorType cursor_type_;
  const IndexedDBDatabase::TaskType task_type_;
  const scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class CountOperation : public IndexedDBTransaction::Operation {
 public:
  CountOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
                 int64 database_id,
                 int64 object_store_id,
                 int64 index_id,
                 scoped_ptr<IndexedDBKeyRange> key_range,
                 scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : backing_store_(backing_store),
        database_id_(database_id),
        object_store_id_(object_store_id),
        index_id_(index_id),
        key_range_(key_range.Pass()),
        callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 database_id_;
  const int64 object_store_id_;
  const int64 index_id_;
  const scoped_ptr<IndexedDBKeyRange> key_range_;
  const scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class DeleteRangeOperation : public IndexedDBTransaction::Operation {
 public:
  DeleteRangeOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
                       int64 database_id,
                       int64 object_store_id,
                       scoped_ptr<IndexedDBKeyRange> key_range,
                       scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : backing_store_(backing_store),
        database_id_(database_id),
        object_store_id_(object_store_id),
        key_range_(key_range.Pass()),
        callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 database_id_;
  const int64 object_store_id_;
  const scoped_ptr<IndexedDBKeyRange> key_range_;
  const scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class ClearOperation : public IndexedDBTransaction::Operation {
 public:
  ClearOperation(scoped_refptr<IndexedDBBackingStore> backing_store,
                 int64 database_id,
                 int64 object_store_id,
                 scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : backing_store_(backing_store),
        database_id_(database_id),
        object_store_id_(object_store_id),
        callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  const scoped_refptr<IndexedDBBackingStore> backing_store_;
  const int64 database_id_;
  const int64 object_store_id_;
  const scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class IndexedDBDatabase::PendingOpenCall {
 public:
  PendingOpenCall(
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks,
      int64 transaction_id,
      int64 version)
      : callbacks_(callbacks),
        database_callbacks_(database_callbacks),
        version_(version),
        transaction_id_(transaction_id) {}
  scoped_refptr<IndexedDBCallbacksWrapper> Callbacks() { return callbacks_; }
  scoped_refptr<IndexedDBDatabaseCallbacksWrapper> DatabaseCallbacks() {
    return database_callbacks_;
  }
  int64 Version() { return version_; }
  int64 TransactionId() const { return transaction_id_; }

 private:
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
  scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks_;
  int64 version_;
  const int64 transaction_id_;
};

class IndexedDBDatabase::PendingDeleteCall {
 public:
  explicit PendingDeleteCall(scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : callbacks_(callbacks) {}
  scoped_refptr<IndexedDBCallbacksWrapper> Callbacks() { return callbacks_; }

 private:
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

scoped_refptr<IndexedDBDatabase> IndexedDBDatabase::Create(
    const string16& name,
    IndexedDBBackingStore* database,
    IndexedDBFactory* factory,
    const string16& unique_identifier) {
  scoped_refptr<IndexedDBDatabase> backend =
      new IndexedDBDatabase(name, database, factory, unique_identifier);
  if (!backend->OpenInternal())
    return 0;
  return backend;
}

namespace {
const base::string16::value_type kNoStringVersion[] = {0};
}

IndexedDBDatabase::IndexedDBDatabase(const string16& name,
                                     IndexedDBBackingStore* backing_store,
                                     IndexedDBFactory* factory,
                                     const string16& unique_identifier)
    : backing_store_(backing_store),
      metadata_(name,
                kInvalidId,
                kNoStringVersion,
                IndexedDBDatabaseMetadata::NO_INT_VERSION,
                kInvalidId),
      identifier_(unique_identifier),
      factory_(factory),
      running_version_change_transaction_(NULL),
      closing_connection_(false) {
  DCHECK(!metadata_.name.empty());
}

void IndexedDBDatabase::AddObjectStore(
    const IndexedDBObjectStoreMetadata& object_store,
    int64 new_max_object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store.id) ==
         metadata_.object_stores.end());
  if (new_max_object_store_id != IndexedDBObjectStoreMetadata::kInvalidId) {
    DCHECK_LT(metadata_.max_object_store_id, new_max_object_store_id);
    metadata_.max_object_store_id = new_max_object_store_id;
  }
  metadata_.object_stores[object_store.id] = object_store;
}

void IndexedDBDatabase::RemoveObjectStore(int64 object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores.erase(object_store_id);
}

void IndexedDBDatabase::AddIndex(int64 object_store_id,
                                 const IndexedDBIndexMetadata& index,
                                 int64 new_max_index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index.id) == object_store.indexes.end());
  object_store.indexes[index.id] = index;
  if (new_max_index_id != IndexedDBIndexMetadata::kInvalidId) {
    DCHECK_LT(object_store.max_index_id, new_max_index_id);
    object_store.max_index_id = new_max_index_id;
  }
  metadata_.object_stores[object_store_id] = object_store;
}

void IndexedDBDatabase::RemoveIndex(int64 object_store_id, int64 index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes.erase(index_id);
  metadata_.object_stores[object_store_id] = object_store;
}

bool IndexedDBDatabase::OpenInternal() {
  bool success = false;
  bool ok = backing_store_->GetIDBDatabaseMetaData(
      metadata_.name, &metadata_, &success);
  DCHECK(success == (metadata_.id != kInvalidId)) << "success = " << success
                                                  << " id = " << metadata_.id;
  if (!ok)
    return false;
  if (success)
    return backing_store_->GetObjectStores(metadata_.id,
                                           &metadata_.object_stores);

  return backing_store_->CreateIDBDatabaseMetaData(
      metadata_.name, metadata_.version, metadata_.int_version, &metadata_.id);
}

IndexedDBDatabase::~IndexedDBDatabase() {
  DCHECK(transactions_.empty());
  DCHECK(pending_open_calls_.empty());
  DCHECK(pending_delete_calls_.empty());
}

scoped_refptr<IndexedDBBackingStore> IndexedDBDatabase::BackingStore() const {
  return backing_store_;
}

void IndexedDBDatabase::CreateObjectStore(int64 transaction_id,
                                          int64 object_store_id,
                                          const string16& name,
                                          const IndexedDBKeyPath& key_path,
                                          bool auto_increment) {
  IDB_TRACE("IndexedDBDatabase::CreateObjectStore");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  DCHECK(metadata_.object_stores.find(object_store_id) ==
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store_metadata(
      name,
      object_store_id,
      key_path,
      auto_increment,
      IndexedDBDatabase::kMinimumIndexId);

  transaction->ScheduleTask(
      new CreateObjectStoreOperation(backing_store_, object_store_metadata),
      new CreateObjectStoreAbortOperation(this, object_store_id));

  AddObjectStore(object_store_metadata, object_store_id);
}

void CreateObjectStoreOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("CreateObjectStoreOperation");
  if (!backing_store_->CreateObjectStore(
          transaction->BackingStoreTransaction(),
          transaction->database()->id(),
          object_store_metadata_.id,
          object_store_metadata_.name,
          object_store_metadata_.key_path,
          object_store_metadata_.auto_increment)) {
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error creating object store '") +
            object_store_metadata_.name + ASCIIToUTF16("'.")));
    return;
  }
}

void IndexedDBDatabase::DeleteObjectStore(int64 transaction_id,
                                          int64 object_store_id) {
  IDB_TRACE("IndexedDBDatabase::DeleteObjectStore");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  transaction->ScheduleTask(
      new DeleteObjectStoreOperation(backing_store_, object_store_metadata),
      new DeleteObjectStoreAbortOperation(this, object_store_metadata));
  RemoveObjectStore(object_store_id);
}

void IndexedDBDatabase::CreateIndex(int64 transaction_id,
                                    int64 object_store_id,
                                    int64 index_id,
                                    const string16& name,
                                    const IndexedDBKeyPath& key_path,
                                    bool unique,
                                    bool multi_entry) {
  IDB_TRACE("IndexedDBDatabase::CreateIndex");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) == object_store.indexes.end());
  const IndexedDBIndexMetadata index_metadata(
      name, index_id, key_path, unique, multi_entry);

  transaction->ScheduleTask(
      new CreateIndexOperation(backing_store_, object_store_id, index_metadata),
      new CreateIndexAbortOperation(this, object_store_id, index_id));

  AddIndex(object_store_id, index_metadata, index_id);
}

void CreateIndexOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("CreateIndexOperation");
  if (!backing_store_->CreateIndex(transaction->BackingStoreTransaction(),
                                   transaction->database()->id(),
                                   object_store_id_,
                                   index_metadata_.id,
                                   index_metadata_.name,
                                   index_metadata_.key_path,
                                   index_metadata_.unique,
                                   index_metadata_.multi_entry)) {
    string16 error_string = ASCIIToUTF16("Internal error creating index '") +
                            index_metadata_.name + ASCIIToUTF16("'.");
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError, error_string));
    return;
  }
}

void CreateIndexAbortOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("CreateIndexAbortOperation");
  DCHECK(!transaction);
  database_->RemoveIndex(object_store_id_, index_id_);
}

void IndexedDBDatabase::DeleteIndex(int64 transaction_id,
                                    int64 object_store_id,
                                    int64 index_id) {
  IDB_TRACE("IndexedDBDatabase::DeleteIndex");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  const IndexedDBIndexMetadata& index_metadata = object_store.indexes[index_id];

  transaction->ScheduleTask(
      new DeleteIndexOperation(backing_store_, object_store_id, index_metadata),
      new DeleteIndexAbortOperation(this, object_store_id, index_metadata));

  RemoveIndex(object_store_id, index_id);
}

void DeleteIndexOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("DeleteIndexOperation");
  bool ok = backing_store_->DeleteIndex(transaction->BackingStoreTransaction(),
                                        transaction->database()->id(),
                                        object_store_id_,
                                        index_metadata_.id);
  if (!ok) {
    string16 error_string = ASCIIToUTF16("Internal error deleting index '") +
                            index_metadata_.name + ASCIIToUTF16("'.");
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError, error_string));
  }
}

void DeleteIndexAbortOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("DeleteIndexAbortOperation");
  DCHECK(!transaction);
  database_->AddIndex(
      object_store_id_, index_metadata_, IndexedDBIndexMetadata::kInvalidId);
}

void IndexedDBDatabase::Commit(int64 transaction_id) {
  // The frontend suggests that we commit, but we may have previously initiated
  // an abort, and so have disposed of the transaction. on_abort has already
  // been dispatched to the frontend, so it will find out about that
  // asynchronously.
  if (transactions_.find(transaction_id) != transactions_.end())
    transactions_[transaction_id]->Commit();
}

void IndexedDBDatabase::Abort(int64 transaction_id) {
  // If the transaction is unknown, then it has already been aborted by the
  // backend before this call so it is safe to ignore it.
  if (transactions_.find(transaction_id) != transactions_.end())
    transactions_[transaction_id]->Abort();
}

void IndexedDBDatabase::Abort(int64 transaction_id,
                              const IndexedDBDatabaseError& error) {
  // If the transaction is unknown, then it has already been aborted by the
  // backend before this call so it is safe to ignore it.
  if (transactions_.find(transaction_id) != transactions_.end())
    transactions_[transaction_id]->Abort(error);
}

void IndexedDBDatabase::Get(
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    bool key_only,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBDatabase::Get");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBDatabaseMetadata::ObjectStoreMap::const_iterator store_iterator =
      metadata_.object_stores.find(object_store_id);
  if (store_iterator == metadata_.object_stores.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;

  transaction->ScheduleTask(new GetOperation(
      backing_store_,
      metadata_.id,
      object_store_id,
      index_id,
      store_iterator->second.key_path,
      store_iterator->second.auto_increment,
      key_range.Pass(),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      callbacks));
}

void GetOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("GetOperation");

  const IndexedDBKey* key;

  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (key_range_->IsOnlyKey()) {
    key = &key_range_->lower();
  } else {
    if (index_id_ == IndexedDBIndexMetadata::kInvalidId) {
      DCHECK_NE(cursor_type_, indexed_db::CURSOR_KEY_ONLY);
      // ObjectStore Retrieval Operation
      backing_store_cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(),
          database_id_,
          object_store_id_,
          *key_range_,
          indexed_db::CURSOR_NEXT);
    } else if (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) {
      // Index Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(),
          database_id_,
          object_store_id_,
          index_id_,
          *key_range_,
          indexed_db::CURSOR_NEXT);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(),
          database_id_,
          object_store_id_,
          index_id_,
          *key_range_,
          indexed_db::CURSOR_NEXT);
    }

    if (!backing_store_cursor) {
      callbacks_->OnSuccess();
      return;
    }

    key = &backing_store_cursor->key();
  }

  scoped_ptr<IndexedDBKey> primary_key;
  bool ok;
  if (index_id_ == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    std::vector<char> value;
    ok = backing_store_->GetRecord(transaction->BackingStoreTransaction(),
                                   database_id_,
                                   object_store_id_,
                                   *key,
                                   &value);
    if (!ok) {
      callbacks_->OnError(
          IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error in GetRecord."));
      return;
    }

    if (value.empty()) {
      callbacks_->OnSuccess();
      return;
    }

    if (auto_increment_ && !key_path_.IsNull()) {
      callbacks_->OnSuccess(&value, *key, key_path_);
      return;
    }

    callbacks_->OnSuccess(&value);
    return;
  }

  // From here we are dealing only with indexes.
  ok = backing_store_->GetPrimaryKeyViaIndex(
      transaction->BackingStoreTransaction(),
      database_id_,
      object_store_id_,
      index_id_,
      *key,
      &primary_key);
  if (!ok) {
    callbacks_->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error in GetPrimaryKeyViaIndex."));
    return;
  }
  if (!primary_key) {
    callbacks_->OnSuccess();
    return;
  }
  if (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) {
    // Index Value Retrieval Operation
    callbacks_->OnSuccess(*primary_key);
    return;
  }

  // Index Referenced Value Retrieval Operation
  std::vector<char> value;
  ok = backing_store_->GetRecord(transaction->BackingStoreTransaction(),
                                 database_id_,
                                 object_store_id_,
                                 *primary_key,
                                 &value);
  if (!ok) {
    callbacks_->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error in GetRecord."));
    return;
  }

  if (value.empty()) {
    callbacks_->OnSuccess();
    return;
  }
  if (auto_increment_ && !key_path_.IsNull()) {
    callbacks_->OnSuccess(&value, *primary_key, key_path_);
    return;
  }
  callbacks_->OnSuccess(&value);
}

static scoped_ptr<IndexedDBKey> GenerateKey(
    scoped_refptr<IndexedDBBackingStore> backing_store,
    scoped_refptr<IndexedDBTransaction> transaction,
    int64 database_id,
    int64 object_store_id) {
  const int64 max_generator_value =
      9007199254740992LL;  // Maximum integer storable as ECMAScript number.
  int64 current_number;
  bool ok = backing_store->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(),
      database_id,
      object_store_id,
      &current_number);
  if (!ok) {
    LOG(ERROR) << "Failed to GetKeyGeneratorCurrentNumber";
    return make_scoped_ptr(new IndexedDBKey());
  }
  if (current_number < 0 || current_number > max_generator_value)
    return make_scoped_ptr(new IndexedDBKey());

  return make_scoped_ptr(
      new IndexedDBKey(current_number, WebIDBKey::NumberType));
}

static bool UpdateKeyGenerator(
    scoped_refptr<IndexedDBBackingStore> backing_store,
    scoped_refptr<IndexedDBTransaction> transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey* key,
    bool check_current) {
  DCHECK(key);
  DCHECK_EQ(WebIDBKey::NumberType, key->type());
  return backing_store->MaybeUpdateKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(),
      database_id,
      object_store_id,
      static_cast<int64>(floor(key->number())) + 1,
      check_current);
}

void IndexedDBDatabase::Put(int64 transaction_id,
                            int64 object_store_id,
                            std::vector<char>* value,
                            scoped_ptr<IndexedDBKey> key,
                            PutMode put_mode,
                            scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
                            const std::vector<int64>& index_ids,
                            const std::vector<IndexKeys>& index_keys) {
  IDB_TRACE("IndexedDBDatabase::Put");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);

  const IndexedDBObjectStoreMetadata object_store_metadata =
      metadata_.object_stores[object_store_id];

  DCHECK(key);
  DCHECK(object_store_metadata.auto_increment || key->IsValid());
  transaction->ScheduleTask(new PutOperation(backing_store_,
                                             id(),
                                             object_store_metadata,
                                             value,
                                             key.Pass(),
                                             put_mode,
                                             callbacks,
                                             index_ids,
                                             index_keys));
}

void PutOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("PutOperation");
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);
  DCHECK_EQ(index_ids_.size(), index_keys_.size());
  bool key_was_generated = false;

  scoped_ptr<IndexedDBKey> key;
  if (put_mode_ != IndexedDBDatabase::CURSOR_UPDATE &&
      object_store_.auto_increment && !key_->IsValid()) {
    scoped_ptr<IndexedDBKey> auto_inc_key = GenerateKey(
        backing_store_, transaction, database_id_, object_store_.id);
    key_was_generated = true;
    if (!auto_inc_key->IsValid()) {
      callbacks_->OnError(
          IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionConstraintError,
                                 "Maximum key generator value reached."));
      return;
    }
    key = auto_inc_key.Pass();
  } else {
    key = key_.Pass();
  }

  DCHECK(key->IsValid());

  IndexedDBBackingStore::RecordIdentifier record_identifier;
  if (put_mode_ == IndexedDBDatabase::ADD_ONLY) {
    bool found = false;
    bool ok = backing_store_->KeyExistsInObjectStore(
        transaction->BackingStoreTransaction(),
        database_id_,
        object_store_.id,
        *key.get(),
        &record_identifier,
        &found);
    if (!ok) {
      callbacks_->OnError(
          IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error checking key existence."));
      return;
    }
    if (found) {
      callbacks_->OnError(
          IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionConstraintError,
                                 "Key already exists in the object store."));
      return;
    }
  }

  ScopedVector<IndexWriter> index_writers;
  string16 error_message;
  bool obeys_constraints = false;
  bool backing_store_success =
      MakeIndexWriters(transaction,
                                                 backing_store_.get(),
                                                 database_id_,
                                                 object_store_,
                                                 *key,
                                                 key_was_generated,
                                                 index_ids_,
                                                 index_keys_,
                                                 &index_writers,
                                                 &error_message,
                                                 &obeys_constraints);
  if (!backing_store_success) {
    callbacks_->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error updating index keys."));
    return;
  }
  if (!obeys_constraints) {
    callbacks_->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionConstraintError, error_message));
    return;
  }

  // Before this point, don't do any mutation. After this point, rollback the
  // transaction in case of error.
  backing_store_success =
      backing_store_->PutRecord(transaction->BackingStoreTransaction(),
                                database_id_,
                                object_store_.id,
                                *key.get(),
                                value_,
                                &record_identifier);
  if (!backing_store_success) {
    callbacks_->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error performing put/add."));
    return;
  }

  for (size_t i = 0; i < index_writers.size(); ++i) {
    IndexWriter* index_writer = index_writers[i];
    index_writer->WriteIndexKeys(record_identifier,
                                 backing_store_.get(),
                                 transaction->BackingStoreTransaction(),
                                 database_id_,
                                 object_store_.id);
  }

  if (object_store_.auto_increment &&
      put_mode_ != IndexedDBDatabase::CURSOR_UPDATE &&
      key->type() == WebIDBKey::NumberType) {
    bool ok = UpdateKeyGenerator(backing_store_,
                                 transaction,
                                 database_id_,
                                 object_store_.id,
                                 key.get(),
                                 !key_was_generated);
    if (!ok) {
      callbacks_->OnError(
          IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error updating key generator."));
      return;
    }
  }

  callbacks_->OnSuccess(*key);
}

void IndexedDBDatabase::SetIndexKeys(int64 transaction_id,
                                     int64 object_store_id,
                                     scoped_ptr<IndexedDBKey> primary_key,
                                     const std::vector<int64>& index_ids,
                                     const std::vector<IndexKeys>& index_keys) {
  IDB_TRACE("IndexedDBDatabase::SetIndexKeys");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  scoped_refptr<IndexedDBBackingStore> store = BackingStore();
  // TODO(alecflett): This method could be asynchronous, but we need to
  // evaluate if it's worth the extra complexity.
  IndexedDBBackingStore::RecordIdentifier record_identifier;
  bool found = false;
  bool ok =
      store->KeyExistsInObjectStore(transaction->BackingStoreTransaction(),
                                    metadata_.id,
                                    object_store_id,
                                    *primary_key,
                                    &record_identifier,
                                    &found);
  if (!ok) {
    transaction->Abort(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error setting index keys."));
    return;
  }
  if (!found) {
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        "Internal error setting index keys for object store."));
    return;
  }

  ScopedVector<IndexWriter> index_writers;
  string16 error_message;
  bool obeys_constraints = false;
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];
  bool backing_store_success = MakeIndexWriters(transaction,
                                                store.get(),
                                                id(),
                                                object_store_metadata,
                                                *primary_key,
                                                false,
                                                index_ids,
                                                index_keys,
                                                &index_writers,
                                                &error_message,
                                                &obeys_constraints);
  if (!backing_store_success) {
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error updating index keys."));
    return;
  }
  if (!obeys_constraints) {
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionConstraintError, error_message));
    return;
  }

  for (size_t i = 0; i < index_writers.size(); ++i) {
    IndexWriter* index_writer = index_writers[i];
    index_writer->WriteIndexKeys(record_identifier,
                                 store.get(),
                                 transaction->BackingStoreTransaction(),
                                 id(),
                                 object_store_id);
  }
}

void IndexedDBDatabase::SetIndexesReady(int64 transaction_id,
                                        int64,
                                        const std::vector<int64>& index_ids) {
  IDB_TRACE("IndexedDBDatabase::SetIndexesReady");

  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;

  transaction->ScheduleTask(IndexedDBDatabase::PREEMPTIVE_TASK,
                            new SetIndexesReadyOperation(index_ids.size()));
}

void SetIndexesReadyOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("SetIndexesReadyOperation");
  for (size_t i = 0; i < index_count_; ++i)
    transaction->DidCompletePreemptiveEvent();
}

void IndexedDBDatabase::OpenCursor(
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorDirection direction,
    bool key_only,
    TaskType task_type,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBDatabase::OpenCursor");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;

  transaction->ScheduleTask(new OpenCursorOperation(
      backing_store_,
      id(),
      object_store_id,
      index_id,
      key_range.Pass(),
      direction,
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      task_type,
      callbacks));
}

void OpenCursorOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("OpenCursorOperation");

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (task_type_ == IndexedDBDatabase::PREEMPTIVE_TASK)
    transaction->AddPreemptiveEvent();

  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (index_id_ == IndexedDBIndexMetadata::kInvalidId) {
    DCHECK_NE(cursor_type_, indexed_db::CURSOR_KEY_ONLY);
    backing_store_cursor = backing_store_->OpenObjectStoreCursor(
        transaction->BackingStoreTransaction(),
        database_id_,
        object_store_id_,
        *key_range_,
        direction_);
  } else {
    DCHECK_EQ(task_type_, IndexedDBDatabase::NORMAL_TASK);
    if (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) {
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(),
          database_id_,
          object_store_id_,
          index_id_,
          *key_range_,
          direction_);
    } else {
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(),
          database_id_,
          object_store_id_,
          index_id_,
          *key_range_,
          direction_);
    }
  }

  if (!backing_store_cursor) {
    callbacks_->OnSuccess(static_cast<std::vector<char>*>(NULL));
    return;
  }

  IndexedDBDatabase::TaskType task_type(
      static_cast<IndexedDBDatabase::TaskType>(task_type_));
  scoped_refptr<IndexedDBCursor> cursor = IndexedDBCursor::Create(
      backing_store_cursor.Pass(), cursor_type_, task_type, transaction);
  callbacks_->OnSuccess(
      cursor, cursor->key(), cursor->primary_key(), cursor->Value());
}

void IndexedDBDatabase::Count(
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBDatabase::Count");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  transaction->ScheduleTask(new CountOperation(backing_store_,
                                               id(),
                                               object_store_id,
                                               index_id,
                                               key_range.Pass(),
                                               callbacks));
}

void CountOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("CountOperation");
  uint32 count = 0;
  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;

  if (index_id_ == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
        transaction->BackingStoreTransaction(),
        database_id_,
        object_store_id_,
        *key_range_,
        indexed_db::CURSOR_NEXT);
  } else {
    backing_store_cursor = backing_store_->OpenIndexKeyCursor(
        transaction->BackingStoreTransaction(),
        database_id_,
        object_store_id_,
        index_id_,
        *key_range_,
        indexed_db::CURSOR_NEXT);
  }
  if (!backing_store_cursor) {
    callbacks_->OnSuccess(count);
    return;
  }

  do {
    ++count;
  } while (backing_store_cursor->ContinueFunction(0));

  callbacks_->OnSuccess(count);
}

void IndexedDBDatabase::DeleteRange(
    int64 transaction_id,
    int64 object_store_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBDatabase::DeleteRange");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;

  transaction->ScheduleTask(new DeleteRangeOperation(
      backing_store_, id(), object_store_id, key_range.Pass(), callbacks));
}

void DeleteRangeOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("DeleteRangeOperation");
  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor =
      backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(),
          database_id_,
          object_store_id_,
          *key_range_,
          indexed_db::CURSOR_NEXT);
  if (backing_store_cursor) {
    do {
      if (!backing_store_->DeleteRecord(
              transaction->BackingStoreTransaction(),
              database_id_,
              object_store_id_,
              backing_store_cursor->record_identifier())) {
        callbacks_->OnError(
            IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error deleting data in range"));
        return;
      }
    } while (backing_store_cursor->ContinueFunction(0));
  }

  callbacks_->OnSuccess();
}

void IndexedDBDatabase::Clear(
    int64 transaction_id,
    int64 object_store_id,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBDatabase::Clear");
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return;
  IndexedDBTransaction* transaction = trans_iterator->second;
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);

  transaction->ScheduleTask(
      new ClearOperation(backing_store_, id(), object_store_id, callbacks));
}

void ClearOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("ObjectStoreClearOperation");
  if (!backing_store_->ClearObjectStore(transaction->BackingStoreTransaction(),
                                        database_id_,
                                        object_store_id_)) {
    callbacks_->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error clearing object store"));
    return;
  }
  callbacks_->OnSuccess();
}

void DeleteObjectStoreOperation::Perform(IndexedDBTransaction* transaction) {
  IDB_TRACE("DeleteObjectStoreOperation");
  bool ok =
      backing_store_->DeleteObjectStore(transaction->BackingStoreTransaction(),
                                        transaction->database()->id(),
                                        object_store_metadata_.id);
  if (!ok) {
    string16 error_string =
        ASCIIToUTF16("Internal error deleting object store '") +
        object_store_metadata_.name + ASCIIToUTF16("'.");
    transaction->Abort(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError, error_string));
  }
}

void IndexedDBDatabase::VersionChangeOperation::Perform(
    IndexedDBTransaction* transaction) {
  IDB_TRACE("VersionChangeOperation");
  int64 database_id = database_->id();
  int64 old_version = database_->metadata_.int_version;
  DCHECK_GT(version_, old_version);
  database_->metadata_.int_version = version_;
  if (!database_->backing_store_->UpdateIDBDatabaseIntVersion(
          transaction->BackingStoreTransaction(),
          database_id,
          database_->metadata_.int_version)) {
    IndexedDBDatabaseError error(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error writing data to stable storage when "
                     "updating version."));
    callbacks_->OnError(error);
    transaction->Abort(error);
    return;
  }
  DCHECK(!database_->pending_second_half_open_);
  database_->pending_second_half_open_.reset(new PendingOpenCall(
      callbacks_, database_callbacks_, transaction_id_, version_));
  callbacks_->OnUpgradeNeeded(old_version, database_, database_->metadata());
}

void IndexedDBDatabase::TransactionStarted(IndexedDBTransaction* transaction) {

  if (transaction->mode() == indexed_db::TRANSACTION_VERSION_CHANGE) {
    DCHECK(!running_version_change_transaction_);
    running_version_change_transaction_ = transaction;
  }
}

void IndexedDBDatabase::TransactionFinished(IndexedDBTransaction* transaction) {

  DCHECK(transactions_.find(transaction->id()) != transactions_.end());
  DCHECK_EQ(transactions_[transaction->id()], transaction);
  transactions_.erase(transaction->id());
  if (transaction->mode() == indexed_db::TRANSACTION_VERSION_CHANGE) {
    DCHECK_EQ(transaction, running_version_change_transaction_);
    running_version_change_transaction_ = NULL;
  }
}

void IndexedDBDatabase::TransactionFinishedAndAbortFired(
    IndexedDBTransaction* transaction) {
  if (transaction->mode() == indexed_db::TRANSACTION_VERSION_CHANGE) {
    if (pending_second_half_open_) {
      pending_second_half_open_->Callbacks()->OnError(
          IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionAbortError,
                                 "Version change transaction was aborted in "
                                 "upgradeneeded event handler."));
      pending_second_half_open_.reset();
    }
    ProcessPendingCalls();
  }
}

void IndexedDBDatabase::TransactionFinishedAndCompleteFired(
    IndexedDBTransaction* transaction) {
  if (transaction->mode() == indexed_db::TRANSACTION_VERSION_CHANGE) {
    DCHECK(pending_second_half_open_);
    if (pending_second_half_open_) {
      DCHECK_EQ(pending_second_half_open_->Version(), metadata_.int_version);
      DCHECK(metadata_.id != kInvalidId);
      pending_second_half_open_->Callbacks()->OnSuccess(this, this->metadata());
      pending_second_half_open_.reset();
    }
    ProcessPendingCalls();
  }
}

size_t IndexedDBDatabase::ConnectionCount() const {
  // This does not include pending open calls, as those should not block version
  // changes and deletes.
  return database_callbacks_set_.size();
}

void IndexedDBDatabase::ProcessPendingCalls() {
  if (pending_second_half_open_) {
    DCHECK_EQ(pending_second_half_open_->Version(), metadata_.int_version);
    DCHECK(metadata_.id != kInvalidId);
    scoped_ptr<PendingOpenCall> pending_call = pending_second_half_open_.Pass();
    pending_call->Callbacks()->OnSuccess(this, this->metadata());
    // Fall through when complete, as pending opens may be unblocked.
  }

  if (pending_run_version_change_transaction_call_ && ConnectionCount() == 1) {
    DCHECK(pending_run_version_change_transaction_call_->Version() >
           metadata_.int_version);
    scoped_ptr<PendingOpenCall> pending_call =
        pending_run_version_change_transaction_call_.Pass();
    RunVersionChangeTransactionFinal(pending_call->Callbacks(),
                                     pending_call->DatabaseCallbacks(),
                                     pending_call->TransactionId(),
                                     pending_call->Version());
    DCHECK_EQ(static_cast<size_t>(1), ConnectionCount());
    // Fall through would be a no-op, since transaction must complete
    // asynchronously.
    DCHECK(IsDeleteDatabaseBlocked());
    DCHECK(IsOpenConnectionBlocked());
    return;
  }

  if (!IsDeleteDatabaseBlocked()) {
    PendingDeleteCallList pending_delete_calls;
    pending_delete_calls_.swap(pending_delete_calls);
    while (!pending_delete_calls.empty()) {
      // Only the first delete call will delete the database, but each must fire
      // callbacks.
      scoped_ptr<PendingDeleteCall> pending_delete_call(
          pending_delete_calls.front());
      pending_delete_calls.pop_front();
      DeleteDatabaseFinal(pending_delete_call->Callbacks());
    }
    // delete_database_final should never re-queue calls.
    DCHECK(pending_delete_calls_.empty());
    // Fall through when complete, as pending opens may be unblocked.
  }

  if (!IsOpenConnectionBlocked()) {
    PendingOpenCallList pending_open_calls;
    pending_open_calls_.swap(pending_open_calls);
    while (!pending_open_calls.empty()) {
      scoped_ptr<PendingOpenCall> pending_open_call(pending_open_calls.front());
      pending_open_calls.pop_front();
      OpenConnection(pending_open_call->Callbacks(),
                     pending_open_call->DatabaseCallbacks(),
                     pending_open_call->TransactionId(),
                     pending_open_call->Version());
    }
  }
}

void IndexedDBDatabase::CreateTransaction(
    int64 transaction_id,
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> callbacks,
    const std::vector<int64>& object_store_ids,
    uint16 mode) {

  DCHECK(database_callbacks_set_.has(callbacks));

  scoped_refptr<IndexedDBTransaction> transaction =
      IndexedDBTransaction::Create(
          transaction_id,
          callbacks,
          object_store_ids,
          static_cast<indexed_db::TransactionMode>(mode),
          this);
  DCHECK(transactions_.find(transaction_id) == transactions_.end());
  transactions_[transaction_id] = transaction.get();
}

bool IndexedDBDatabase::IsOpenConnectionBlocked() const {
  return !pending_delete_calls_.empty() ||
         running_version_change_transaction_ ||
         pending_run_version_change_transaction_call_;
}

void IndexedDBDatabase::OpenConnection(
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks,
    int64 transaction_id,
    int64 version) {
  DCHECK(backing_store_.get());

  // TODO(jsbell): Should have a priority queue so that higher version
  // requests are processed first. http://crbug.com/225850
  if (IsOpenConnectionBlocked()) {
    pending_open_calls_.push_back(new PendingOpenCall(
        callbacks, database_callbacks, transaction_id, version));
    return;
  }

  if (metadata_.id == kInvalidId) {
    // The database was deleted then immediately re-opened; OpenInternal()
    // recreates it in the backing store.
    if (OpenInternal()) {
      DCHECK_EQ(metadata_.int_version,
                IndexedDBDatabaseMetadata::NO_INT_VERSION);
    } else {
      string16 message;
      if (version == IndexedDBDatabaseMetadata::NO_INT_VERSION)
        message = ASCIIToUTF16(
            "Internal error opening database with no version specified.");
      else
        message =
            ASCIIToUTF16("Internal error opening database with version ") +
            Int64ToString16(version);
      callbacks->OnError(IndexedDBDatabaseError(
          WebKit::WebIDBDatabaseExceptionUnknownError, message));
      return;
    }
  }

  // We infer that the database didn't exist from its lack of either type of
  // version.
  bool is_new_database =
      metadata_.version == kNoStringVersion &&
      metadata_.int_version == IndexedDBDatabaseMetadata::NO_INT_VERSION;

  if (version == IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION) {
    // For unit tests only - skip upgrade steps. Calling from script with
    // DEFAULT_INT_VERSION throws exception.
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
    DCHECK(is_new_database);
    database_callbacks_set_.insert(database_callbacks);
    callbacks->OnSuccess(this, this->metadata());
    return;
  }

  if (version == IndexedDBDatabaseMetadata::NO_INT_VERSION) {
    if (!is_new_database) {
      database_callbacks_set_.insert(database_callbacks);
      callbacks->OnSuccess(this, this->metadata());
      return;
    }
    // Spec says: If no version is specified and no database exists, set
    // database version to 1.
    version = 1;
  }

  if (version > metadata_.int_version) {
    database_callbacks_set_.insert(database_callbacks);
    RunVersionChangeTransaction(
        callbacks, database_callbacks, transaction_id, version);
    return;
  }
  if (version < metadata_.int_version) {
    callbacks->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionVersionError,
        ASCIIToUTF16("The requested version (") + Int64ToString16(version) +
            ASCIIToUTF16(") is less than the existing version (") +
            Int64ToString16(metadata_.int_version) + ASCIIToUTF16(").")));
    return;
  }
  DCHECK_EQ(version, metadata_.int_version);
  database_callbacks_set_.insert(database_callbacks);
  callbacks->OnSuccess(this, this->metadata());
}

void IndexedDBDatabase::RunVersionChangeTransaction(
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks,
    int64 transaction_id,
    int64 requested_version) {

  DCHECK(callbacks.get());
  DCHECK(database_callbacks_set_.has(database_callbacks));
  if (ConnectionCount() > 1) {
    // Front end ensures the event is not fired at connections that have
    // close_pending set.
    for (DatabaseCallbacksSet::const_iterator it =
             database_callbacks_set_.begin();
         it != database_callbacks_set_.end();
         ++it) {
      if (it->get() != database_callbacks.get())
        (*it)->OnVersionChange(metadata_.int_version, requested_version);
    }
    // TODO(jsbell): Remove the call to OnBlocked and instead wait
    // until the frontend tells us that all the "versionchange" events
    // have been delivered.  http://crbug.com/100123
    callbacks->OnBlocked(metadata_.int_version);

    DCHECK(!pending_run_version_change_transaction_call_);
    pending_run_version_change_transaction_call_.reset(new PendingOpenCall(
        callbacks, database_callbacks, transaction_id, requested_version));
    return;
  }
  RunVersionChangeTransactionFinal(
      callbacks, database_callbacks, transaction_id, requested_version);
}

void IndexedDBDatabase::RunVersionChangeTransactionFinal(
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks,
    int64 transaction_id,
    int64 requested_version) {

  std::vector<int64> object_store_ids;
  CreateTransaction(transaction_id,
                    database_callbacks,
                    object_store_ids,
                    indexed_db::TRANSACTION_VERSION_CHANGE);
  scoped_refptr<IndexedDBTransaction> transaction =
      transactions_[transaction_id];

  transaction->ScheduleTask(
      new VersionChangeOperation(this,
                                 transaction_id,
                                 requested_version,
                                 callbacks,
                                 database_callbacks),
      new VersionChangeAbortOperation(
          this, metadata_.version, metadata_.int_version));

  DCHECK(!pending_second_half_open_);
}

void IndexedDBDatabase::DeleteDatabase(
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {

  if (IsDeleteDatabaseBlocked()) {
    for (DatabaseCallbacksSet::const_iterator it =
             database_callbacks_set_.begin();
         it != database_callbacks_set_.end();
         ++it) {
      // Front end ensures the event is not fired at connections that have
      // close_pending set.
      (*it)->OnVersionChange(metadata_.int_version,
                             IndexedDBDatabaseMetadata::NO_INT_VERSION);
    }
    // TODO(jsbell): Only fire OnBlocked if there are open
    // connections after the VersionChangeEvents are received, not
    // just set up to fire.  http://crbug.com/100123
    callbacks->OnBlocked(metadata_.int_version);
    pending_delete_calls_.push_back(new PendingDeleteCall(callbacks));
    return;
  }
  DeleteDatabaseFinal(callbacks);
}

bool IndexedDBDatabase::IsDeleteDatabaseBlocked() const {
  return !!ConnectionCount();
}

void IndexedDBDatabase::DeleteDatabaseFinal(
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  DCHECK(!IsDeleteDatabaseBlocked());
  DCHECK(backing_store_.get());
  if (!backing_store_->DeleteDatabase(metadata_.name)) {
    callbacks->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error deleting database."));
    return;
  }
  metadata_.version = kNoStringVersion;
  metadata_.id = kInvalidId;
  metadata_.int_version = IndexedDBDatabaseMetadata::NO_INT_VERSION;
  metadata_.object_stores.clear();
  callbacks->OnSuccess();
}

void IndexedDBDatabase::Close(
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> callbacks) {
  DCHECK(callbacks.get());
  DCHECK(database_callbacks_set_.has(callbacks));

  // Close outstanding transactions from the closing connection. This
  // can not happen if the close is requested by the connection itself
  // as the front-end defers the close until all transactions are
  // complete, so something unusual has happened e.g. unexpected
  // process termination.
  {
    TransactionMap transactions(transactions_);
    for (TransactionMap::const_iterator it = transactions.begin(),
                                        end = transactions.end();
         it != end;
         ++it) {
      if (it->second->connection() == callbacks.get())
        it->second->Abort(
            IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                                   "Connection is closing."));
    }
  }

  database_callbacks_set_.erase(callbacks);
  if (pending_second_half_open_ &&
      pending_second_half_open_->DatabaseCallbacks().get() == callbacks.get()) {
    pending_second_half_open_->Callbacks()->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionAbortError,
                               "The connection was closed."));
    pending_second_half_open_.reset();
  }

  // process_pending_calls allows the inspector to process a pending open call
  // and call close, reentering IndexedDBDatabase::close. Then the
  // backend would be removed both by the inspector closing its connection, and
  // by the connection that first called close.
  // To avoid that situation, don't proceed in case of reentrancy.
  if (closing_connection_)
    return;
  base::AutoReset<bool> ClosingConnection(&closing_connection_, true);
  ProcessPendingCalls();

  // TODO(jsbell): Add a test for the pending_open_calls_ cases below.
  if (!ConnectionCount() && !pending_open_calls_.size() &&
      !pending_delete_calls_.size()) {
    DCHECK(transactions_.empty());

    backing_store_ = NULL;

    // factory_ should only be null in unit tests.
    DCHECK(factory_ ||
           !BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
    if (factory_.get())
      factory_->RemoveIDBDatabaseBackend(identifier_);
  }
}

void CreateObjectStoreAbortOperation::Perform(
    IndexedDBTransaction* transaction) {
  IDB_TRACE("CreateObjectStoreAbortOperation");
  DCHECK(!transaction);
  database_->RemoveObjectStore(object_store_id_);
}

void DeleteObjectStoreAbortOperation::Perform(
    IndexedDBTransaction* transaction) {
  IDB_TRACE("DeleteObjectStoreAbortOperation");
  DCHECK(!transaction);
  database_->AddObjectStore(object_store_metadata_,
                            IndexedDBObjectStoreMetadata::kInvalidId);
}

void IndexedDBDatabase::VersionChangeAbortOperation::Perform(
    IndexedDBTransaction* transaction) {
  IDB_TRACE("VersionChangeAbortOperation");
  DCHECK(!transaction);
  database_->metadata_.version = previous_version_;
  database_->metadata_.int_version = previous_int_version_;
}

}  // namespace content
