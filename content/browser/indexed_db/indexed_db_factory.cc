// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"

namespace content {

template <typename K, typename M>
static void CleanWeakMap(std::map<K, base::WeakPtr<M> >* map) {
  std::map<K, base::WeakPtr<M> > other;
  other.swap(*map);

  typename std::map<K, base::WeakPtr<M> >::const_iterator iter = other.begin();
  while (iter != other.end()) {
    if (iter->second.get())
      (*map)[iter->first] = iter->second;
    ++iter;
  }
}

static std::string ComputeFileIdentifier(const std::string& origin_identifier) {
  return origin_identifier + "@1";
}

IndexedDBFactory::IndexedDBFactory() {}

IndexedDBFactory::~IndexedDBFactory() {}

void IndexedDBFactory::RemoveIDBDatabaseBackend(
    const IndexedDBDatabase::Identifier& unique_identifier) {
  DCHECK(database_backend_map_.find(unique_identifier) !=
         database_backend_map_.end());
  database_backend_map_.erase(unique_identifier);
}

void IndexedDBFactory::GetDatabaseNames(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const std::string& origin_identifier,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::GetDatabaseNames");
  // TODO(dgrogan): Plumb data_loss back to script eventually?
  WebKit::WebIDBCallbacks::DataLoss data_loss;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_identifier, data_directory, &data_loss);
  if (!backing_store) {
    callbacks->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error opening backing store for "
                               "indexedDB.webkitGetDatabaseNames."));
    return;
  }

  callbacks->OnSuccess(backing_store->GetDatabaseNames());
}

void IndexedDBFactory::DeleteDatabase(
    const string16& name,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const std::string& origin_identifier,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(origin_identifier, name);
  IndexedDBDatabaseMap::iterator it =
      database_backend_map_.find(unique_identifier);
  if (it != database_backend_map_.end()) {
    // If there are any connections to the database, directly delete the
    // database.
    it->second->DeleteDatabase(callbacks);
    return;
  }

  // TODO(dgrogan): Plumb data_loss back to script eventually?
  WebKit::WebIDBCallbacks::DataLoss data_loss;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_identifier, data_directory, &data_loss);
  if (!backing_store) {
    callbacks->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error opening backing store "
                     "for indexedDB.deleteDatabase.")));
    return;
  }

  scoped_refptr<IndexedDBDatabase> database_backend =
      IndexedDBDatabase::Create(name, backing_store, this, unique_identifier);
  if (!database_backend) {
    callbacks->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error creating database backend for "
                     "indexedDB.deleteDatabase.")));
    return;
  }

  database_backend_map_[unique_identifier] = database_backend;
  database_backend->DeleteDatabase(callbacks);
  database_backend_map_.erase(unique_identifier);
}

scoped_refptr<IndexedDBBackingStore> IndexedDBFactory::OpenBackingStore(
    const std::string& origin_identifier,
    const base::FilePath& data_directory,
    WebKit::WebIDBCallbacks::DataLoss* data_loss) {
  const std::string file_identifier = ComputeFileIdentifier(origin_identifier);
  const bool open_in_memory = data_directory.empty();

  IndexedDBBackingStoreMap::iterator it2 =
      backing_store_map_.find(file_identifier);
  if (it2 != backing_store_map_.end() && it2->second.get())
    return it2->second.get();

  scoped_refptr<IndexedDBBackingStore> backing_store;
  if (open_in_memory) {
    backing_store = IndexedDBBackingStore::OpenInMemory(file_identifier);
  } else {
    backing_store = IndexedDBBackingStore::Open(
        origin_identifier, data_directory, file_identifier, data_loss);
  }

  if (backing_store.get()) {
    CleanWeakMap(&backing_store_map_);
    backing_store_map_[file_identifier] = backing_store->GetWeakPtr();
    // If an in-memory database, bind lifetime to this factory instance.
    if (open_in_memory)
      session_only_backing_stores_.insert(backing_store);

    // All backing stores associated with this factory should be of the same
    // type.
    DCHECK(session_only_backing_stores_.empty() || open_in_memory);

    return backing_store;
  }

  return 0;
}

void IndexedDBFactory::Open(
    const string16& name,
    int64 version,
    int64 transaction_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const std::string& origin_identifier,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::Open");
  scoped_refptr<IndexedDBDatabase> database_backend;
  IndexedDBDatabase::Identifier unique_identifier(origin_identifier, name);
  IndexedDBDatabaseMap::iterator it =
      database_backend_map_.find(unique_identifier);
  WebKit::WebIDBCallbacks::DataLoss data_loss =
      WebKit::WebIDBCallbacks::DataLossNone;
  if (it == database_backend_map_.end()) {
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin_identifier, data_directory, &data_loss);
    if (!backing_store) {
      callbacks->OnError(IndexedDBDatabaseError(
          WebKit::WebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16(
              "Internal error opening backing store for indexedDB.open.")));
      return;
    }

    database_backend =
        IndexedDBDatabase::Create(name, backing_store, this, unique_identifier);
    if (!database_backend) {
      callbacks->OnError(IndexedDBDatabaseError(
          WebKit::WebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16(
              "Internal error creating database backend for indexedDB.open.")));
      return;
    }

    database_backend_map_[unique_identifier] = database_backend;
  } else {
    database_backend = it->second;
  }

  database_backend->OpenConnection(
      callbacks, database_callbacks, transaction_id, version, data_loss);
}

std::vector<IndexedDBDatabase*> IndexedDBFactory::GetOpenDatabasesForOrigin(
    const std::string& origin_identifier) const {
  std::vector<IndexedDBDatabase*> result;
  for (IndexedDBDatabaseMap::const_iterator it = database_backend_map_.begin();
       it != database_backend_map_.end(); ++it) {
    if (it->first.first == origin_identifier)
      result.push_back(it->second.get());
  }
  return result;
}

}  // namespace content
