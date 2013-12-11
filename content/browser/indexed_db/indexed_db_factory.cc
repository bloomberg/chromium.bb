// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"
#include "webkit/common/database/database_identifier.h"

namespace content {

const int64 kBackingStoreGracePeriodMs = 2000;

IndexedDBFactory::IndexedDBFactory(IndexedDBContextImpl* context)
    : context_(context) {}

IndexedDBFactory::~IndexedDBFactory() {}

void IndexedDBFactory::ReleaseDatabase(
    const IndexedDBDatabase::Identifier& identifier,
    const GURL& origin_url,
    bool forcedClose) {
  IndexedDBDatabaseMap::iterator it = database_map_.find(identifier);
  DCHECK(it != database_map_.end());
  DCHECK(!it->second->backing_store());
  database_map_.erase(it);

  // No grace period on a forced-close, as the initiator is
  // assuming the backing store will be released once all
  // connections are closed.
  ReleaseBackingStore(origin_url, forcedClose);
}

void IndexedDBFactory::ReleaseBackingStore(const GURL& origin_url,
                                           bool immediate) {
  // Only close if this is the last reference.
  if (!HasLastBackingStoreReference(origin_url))
    return;

  // If this factory does hold the last reference to the backing store, it can
  // be closed - but unless requested to close it immediately, keep it around
  // for a short period so that a re-open is fast.
  if (immediate) {
    CloseBackingStore(origin_url);
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!backing_store_map_[origin_url]->close_timer()->IsRunning());
  backing_store_map_[origin_url]->close_timer()->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kBackingStoreGracePeriodMs),
      base::Bind(&IndexedDBFactory::MaybeCloseBackingStore, this, origin_url));
}

void IndexedDBFactory::MaybeCloseBackingStore(const GURL& origin_url) {
  // Another reference may have opened since the maybe-close was posted, so it
  // is necessary to check again.
  if (HasLastBackingStoreReference(origin_url))
    CloseBackingStore(origin_url);
}

void IndexedDBFactory::CloseBackingStore(const GURL& origin_url) {
  IndexedDBBackingStoreMap::iterator it = backing_store_map_.find(origin_url);
  DCHECK(it != backing_store_map_.end());
  // Stop the timer (if it's running) - this may happen if the timer was started
  // and then a forced close occurs.
  it->second->close_timer()->Stop();
  backing_store_map_.erase(it);
}

bool IndexedDBFactory::HasLastBackingStoreReference(const GURL& origin_url)
    const {
  IndexedDBBackingStore* ptr;
  {
    // Scope so that the implicit scoped_refptr<> is freed.
    IndexedDBBackingStoreMap::const_iterator it =
        backing_store_map_.find(origin_url);
    DCHECK(it != backing_store_map_.end());
    ptr = it->second.get();
  }
  return ptr->HasOneRef();
}

void IndexedDBFactory::ForceClose(const GURL& origin_url) {
  if (backing_store_map_.find(origin_url) != backing_store_map_.end())
    ReleaseBackingStore(origin_url, true /* immediate */);
}

void IndexedDBFactory::ContextDestroyed() {
  // Timers on backing stores hold a reference to this factory. When the
  // context (which nominally owns this factory) is destroyed during thread
  // termination the timers must be stopped so that this factory and the
  // stores can be disposed of.
  for (IndexedDBBackingStoreMap::iterator it = backing_store_map_.begin();
       it != backing_store_map_.end();
       ++it)
    it->second->close_timer()->Stop();
  backing_store_map_.clear();
  context_ = NULL;
}

void IndexedDBFactory::GetDatabaseNames(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const GURL& origin_url,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::GetDatabaseNames");
  // TODO(dgrogan): Plumb data_loss back to script eventually?
  blink::WebIDBDataLoss data_loss;
  std::string data_loss_message;
  bool disk_full;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_url,
                       data_directory,
                       &data_loss,
                       &data_loss_message,
                       &disk_full);
  if (!backing_store) {
    callbacks->OnError(
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error opening backing store for "
                               "indexedDB.webkitGetDatabaseNames."));
    return;
  }

  callbacks->OnSuccess(backing_store->GetDatabaseNames());
  ReleaseBackingStore(origin_url, false /* immediate */);
}

void IndexedDBFactory::DeleteDatabase(
    const base::string16& name,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const GURL& origin_url,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(origin_url, name);
  IndexedDBDatabaseMap::iterator it = database_map_.find(unique_identifier);
  if (it != database_map_.end()) {
    // If there are any connections to the database, directly delete the
    // database.
    it->second->DeleteDatabase(callbacks);
    return;
  }

  // TODO(dgrogan): Plumb data_loss back to script eventually?
  blink::WebIDBDataLoss data_loss;
  std::string data_loss_message;
  bool disk_full = false;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_url,
                       data_directory,
                       &data_loss,
                       &data_loss_message,
                       &disk_full);
  if (!backing_store) {
    callbacks->OnError(
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               ASCIIToUTF16(
                                   "Internal error opening backing store "
                                   "for indexedDB.deleteDatabase.")));
    return;
  }

  scoped_refptr<IndexedDBDatabase> database =
      IndexedDBDatabase::Create(name, backing_store, this, unique_identifier);
  if (!database) {
    callbacks->OnError(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16(
            "Internal error creating database backend for "
            "indexedDB.deleteDatabase.")));
    return;
  }

  database_map_[unique_identifier] = database;
  database->DeleteDatabase(callbacks);
  database_map_.erase(unique_identifier);
  ReleaseBackingStore(origin_url, false /* immediate */);
}

void IndexedDBFactory::HandleBackingStoreFailure(const GURL& origin_url) {
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->ForceClose(origin_url);
}

bool IndexedDBFactory::IsDatabaseOpen(const GURL& origin_url,
                                      const base::string16& name) const {

  return !!database_map_.count(IndexedDBDatabase::Identifier(origin_url, name));
}

bool IndexedDBFactory::IsBackingStoreOpen(const GURL& origin_url) const {
  return backing_store_map_.find(origin_url) != backing_store_map_.end();
}

bool IndexedDBFactory::IsBackingStorePendingClose(const GURL& origin_url)
    const {
  IndexedDBBackingStoreMap::const_iterator it =
      backing_store_map_.find(origin_url);
  if (it == backing_store_map_.end())
    return false;
  return it->second->close_timer()->IsRunning();
}

scoped_refptr<IndexedDBBackingStore> IndexedDBFactory::OpenBackingStore(
    const GURL& origin_url,
    const base::FilePath& data_directory,
    blink::WebIDBDataLoss* data_loss,
    std::string* data_loss_message,
    bool* disk_full) {
  const bool open_in_memory = data_directory.empty();

  IndexedDBBackingStoreMap::iterator it2 = backing_store_map_.find(origin_url);
  if (it2 != backing_store_map_.end()) {
    it2->second->close_timer()->Stop();
    return it2->second;
  }

  scoped_refptr<IndexedDBBackingStore> backing_store;
  if (open_in_memory) {
    backing_store = IndexedDBBackingStore::OpenInMemory(origin_url);
  } else {
    backing_store = IndexedDBBackingStore::Open(origin_url,
                                                data_directory,
                                                data_loss,
                                                data_loss_message,
                                                disk_full);
  }

  if (backing_store.get()) {
    backing_store_map_[origin_url] = backing_store;
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
    const base::string16& name,
    int64 version,
    int64 transaction_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const GURL& origin_url,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::Open");
  scoped_refptr<IndexedDBDatabase> database;
  IndexedDBDatabase::Identifier unique_identifier(origin_url, name);
  IndexedDBDatabaseMap::iterator it = database_map_.find(unique_identifier);
  blink::WebIDBDataLoss data_loss =
      blink::WebIDBDataLossNone;
  std::string data_loss_message;
  bool disk_full = false;
  bool was_open = (it != database_map_.end());
  if (!was_open) {
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin_url,
                         data_directory,
                         &data_loss,
                         &data_loss_message,
                         &disk_full);
    if (!backing_store) {
      if (disk_full) {
        callbacks->OnError(
            IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionQuotaError,
                                   ASCIIToUTF16(
                                       "Encountered full disk while opening "
                                       "backing store for indexedDB.open.")));
        return;
      }
      callbacks->OnError(IndexedDBDatabaseError(
          blink::WebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16(
              "Internal error opening backing store for indexedDB.open.")));
      return;
    }

    database =
        IndexedDBDatabase::Create(name, backing_store, this, unique_identifier);
    if (!database) {
      callbacks->OnError(IndexedDBDatabaseError(
          blink::WebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16(
              "Internal error creating database backend for indexedDB.open.")));
      return;
    }
  } else {
    database = it->second;
  }

  database->OpenConnection(callbacks,
                           database_callbacks,
                           transaction_id,
                           version,
                           data_loss,
                           data_loss_message);

  if (!was_open && database->ConnectionCount() > 0)
    database_map_[unique_identifier] = database;
}

std::vector<IndexedDBDatabase*> IndexedDBFactory::GetOpenDatabasesForOrigin(
    const GURL& origin_url) const {
  std::vector<IndexedDBDatabase*> result;
  for (IndexedDBDatabaseMap::const_iterator it = database_map_.begin();
       it != database_map_.end();
       ++it) {
    if (it->first.first == origin_url)
      result.push_back(it->second.get());
  }
  return result;
}

}  // namespace content
