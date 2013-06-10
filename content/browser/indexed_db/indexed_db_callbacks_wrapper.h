// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_WRAPPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"

namespace content {
class IndexedDBCallbacksBase;
class IndexedDBDatabaseCallbacksWrapper;
class IndexedDBCursor;
class IndexedDBDatabase;
class WebIDBCursorImpl;
class WebIDBDatabaseImpl;
struct IndexedDBDatabaseMetadata;

class CONTENT_EXPORT IndexedDBCallbacksWrapper
    : public base::RefCounted<IndexedDBCallbacksWrapper> {
 public:
  static scoped_refptr<IndexedDBCallbacksWrapper> Create(
      IndexedDBCallbacksBase* callbacks) {
    return make_scoped_refptr(new IndexedDBCallbacksWrapper(callbacks));
  }

  virtual void OnError(const IndexedDBDatabaseError& error);
  // From IDBFactory.webkitGetDatabaseNames()
  virtual void OnSuccess(const std::vector<string16>& string);
  // From IDBObjectStore/IDBIndex.openCursor(),
  // IDBIndex.openKeyCursor()
  virtual void OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         std::vector<char>* value);
  // From IDBObjectStore.add()/put(), IDBIndex.getKey()
  virtual void OnSuccess(const IndexedDBKey& key);
  // From IDBObjectStore/IDBIndex.get()/count(), and various methods
  // that yield null/undefined.
  virtual void OnSuccess(std::vector<char>* value);
  // From IDBObjectStore/IDBIndex.get() (with key injection)
  virtual void OnSuccess(std::vector<char>* data,
                         const IndexedDBKey& key,
                         const IndexedDBKeyPath& key_path);
  // From IDBObjectStore/IDBIndex.count()
  virtual void OnSuccess(int64 value);

  // From IDBFactor.deleteDatabase(),
  // IDBObjectStore/IDBIndex.get(), IDBObjectStore.delete(),
  // IDBObjectStore.clear()
  virtual void OnSuccess();

  // From IDBCursor.advance()/continue()
  virtual void OnSuccess(const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         std::vector<char>* value);
  // From IDBCursor.advance()/continue()
  virtual void OnSuccessWithPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      const std::vector<std::vector<char> >& values);
  // From IDBFactory.open()/deleteDatabase()
  virtual void OnBlocked(int64 existing_version);
  // From IDBFactory.open()
  virtual void OnUpgradeNeeded(
      int64 old_version,
      scoped_refptr<IndexedDBDatabase> db,
      const content::IndexedDBDatabaseMetadata& metadata);
  virtual void OnSuccess(scoped_refptr<IndexedDBDatabase> db,
                         const content::IndexedDBDatabaseMetadata& metadata);
  virtual void SetDatabaseCallbacks(
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks);

 protected:
  virtual ~IndexedDBCallbacksWrapper();
  explicit IndexedDBCallbacksWrapper(IndexedDBCallbacksBase* callbacks);

 private:
  friend class base::RefCounted<IndexedDBCallbacksWrapper>;
  scoped_ptr<WebIDBDatabaseImpl> web_database_impl_;
  scoped_ptr<IndexedDBCallbacksBase> callbacks_;
  scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks_;
  bool did_complete_;
  bool did_create_proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_WRAPPER_H_
