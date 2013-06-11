// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks_wrapper.h"
#include "third_party/WebKit/public/platform/WebIDBDatabase.h"

namespace WebKit {
class WebIDBDatabaseError;
class WebIDBDatabaseMetadata;
}

namespace content {
class IndexedDBCallbacksBase;
class IndexedDBDatabase;
class IndexedDBDatabaseCallbacks;
class IndexedDBDatabaseCallbacksWrapper;

class CONTENT_EXPORT WebIDBDatabaseImpl {
 public:
  WebIDBDatabaseImpl(
      scoped_refptr<IndexedDBDatabase> db,
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> callbacks);
  virtual ~WebIDBDatabaseImpl();

  typedef std::vector<IndexedDBKey> IndexKeys;

  virtual void createObjectStore(long long transaction_id,
                                 long long object_store_id,
                                 const WebKit::WebString& name,
                                 const IndexedDBKeyPath& key_path,
                                 bool auto_increment);
  virtual void deleteObjectStore(long long object_store_id,
                                 long long transaction_id);
  virtual void createTransaction(long long id,
                                 IndexedDBDatabaseCallbacks* callbacks,
                                 const std::vector<int64>& scope,
                                 unsigned short mode);
  virtual void forceClose();
  virtual void close();
  virtual void abort(long long transaction_id);
  virtual void abort(long long transaction_id,
                     const WebKit::WebIDBDatabaseError& error);
  virtual void commit(long long transaction_id);

  virtual void get(long long transaction_id,
                   long long object_store_id,
                   long long index_id,
                   const IndexedDBKeyRange& range,
                   bool key_only,
                   IndexedDBCallbacksBase* callbacks);
  virtual void put(long long transaction_id,
                   long long object_store_id,
                   std::vector<char>* value,
                   const IndexedDBKey& key,
                   WebKit::WebIDBDatabase::PutMode mode,
                   IndexedDBCallbacksBase* callbacks,
                   const std::vector<int64>& index_ids,
                   const std::vector<IndexKeys>& index_keys);
  virtual void setIndexKeys(long long transaction_id,
                            long long object_store_id,
                            const IndexedDBKey& key,
                            const std::vector<int64>& index_ids,
                            const std::vector<IndexKeys>& index_keys);
  virtual void setIndexesReady(long long transaction_id,
                               long long object_store_id,
                               const std::vector<int64>& index_ids);
  virtual void openCursor(long long transaction_id,
                          long long object_store_id,
                          long long index_id,
                          const IndexedDBKeyRange& range,
                          unsigned short direction,
                          bool key_only,
                          WebKit::WebIDBDatabase::TaskType task_type,
                          IndexedDBCallbacksBase* callbacks);
  virtual void count(long long transaction_id,
                     long long object_store_id,
                     long long index_id,
                     const IndexedDBKeyRange& range,
                     IndexedDBCallbacksBase* callbacks);
  virtual void deleteRange(long long transaction_id,
                           long long object_store_id,
                           const IndexedDBKeyRange& range,
                           IndexedDBCallbacksBase* callbacks);
  virtual void clear(long long transaction_id,
                     long long object_store_id,
                     IndexedDBCallbacksBase* callbacks);

  virtual void createIndex(long long transaction_id,
                           long long object_store_id,
                           long long index_id,
                           const WebKit::WebString& name,
                           const IndexedDBKeyPath& key_path,
                           bool unique,
                           bool multi_entry);
  virtual void deleteIndex(long long transaction_id,
                           long long object_store_id,
                           long long index_id);

 private:
  scoped_refptr<IndexedDBDatabase> database_backend_;
  scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
