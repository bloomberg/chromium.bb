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
class WebIDBDatabaseCallbacks;
class WebIDBDatabaseError;
class WebIDBDatabaseMetadata;
}

namespace content {
class IndexedDBDatabase;
class IndexedDBDatabaseCallbacksWrapper;

// See comment in WebIDBFactory for a high level overview these classes.
class CONTENT_EXPORT WebIDBDatabaseImpl
    : NON_EXPORTED_BASE(public WebKit::WebIDBDatabase) {
 public:
  WebIDBDatabaseImpl(
      scoped_refptr<IndexedDBDatabase> db,
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> callbacks);
  virtual ~WebIDBDatabaseImpl();

  virtual void createObjectStore(long long transaction_id,
                                 long long object_store_id,
                                 const WebKit::WebString& name,
                                 const WebKit::WebIDBKeyPath& key_path,
                                 bool auto_increment);
  virtual void deleteObjectStore(long long object_store_id,
                                 long long transaction_id);
  virtual void createTransaction(long long id,
                                 WebKit::WebIDBDatabaseCallbacks* callbacks,
                                 const WebKit::WebVector<long long>& scope,
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
                   const WebKit::WebIDBKeyRange& range,
                   bool key_only,
                   WebKit::WebIDBCallbacks* callbacks);
  virtual void put(long long transaction_id,
                   long long object_store_id,
                   const WebKit::WebData& value,
                   const WebKit::WebIDBKey& key,
                   PutMode mode,
                   WebKit::WebIDBCallbacks* callbacks,
                   const WebKit::WebVector<long long>& index_ids,
                   const WebKit::WebVector<WebIndexKeys>& index_keys);
  virtual void setIndexKeys(long long transaction_id,
                            long long object_store_id,
                            const WebKit::WebIDBKey& key,
                            const WebKit::WebVector<long long>& index_ids,
                            const WebKit::WebVector<WebIndexKeys>& index_keys);
  virtual void setIndexesReady(long long transaction_id,
                               long long object_store_id,
                               const WebKit::WebVector<long long>& index_ids);
  virtual void openCursor(long long transaction_id,
                          long long object_store_id,
                          long long index_id,
                          const WebKit::WebIDBKeyRange& range,
                          unsigned short direction,
                          bool key_only,
                          TaskType task_type,
                          WebKit::WebIDBCallbacks* callbacks);
  virtual void count(long long transaction_id,
                     long long object_store_id,
                     long long index_id,
                     const WebKit::WebIDBKeyRange& range,
                     WebKit::WebIDBCallbacks* callbacks);
  virtual void deleteRange(long long transaction_id,
                           long long object_store_id,
                           const WebKit::WebIDBKeyRange& range,
                           WebKit::WebIDBCallbacks* callbacks);
  virtual void clear(long long transaction_id,
                     long long object_store_id,
                     WebKit::WebIDBCallbacks* callbacks);

  virtual void createIndex(long long transaction_id,
                           long long object_store_id,
                           long long index_id,
                           const WebKit::WebString& name,
                           const WebKit::WebIDBKeyPath& key_path,
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
