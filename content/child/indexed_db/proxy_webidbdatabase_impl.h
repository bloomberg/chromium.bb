// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_PROXY_WEBIDBDATABASE_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_PROXY_WEBIDBDATABASE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebIDBDatabase.h"

namespace WebKit {
class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebString;
}

namespace content {
class ThreadSafeSender;

class RendererWebIDBDatabaseImpl : public WebKit::WebIDBDatabase {
 public:
  RendererWebIDBDatabaseImpl(int32 ipc_database_id,
                             int32 ipc_database_callbacks_id,
                             ThreadSafeSender* thread_safe_sender);
  virtual ~RendererWebIDBDatabaseImpl();

  // WebKit::WebIDBDatabase
  virtual void createObjectStore(
      long long transaction_id,
      long long objectstore_id,
      const WebKit::WebString& name,
      const WebKit::WebIDBKeyPath& key_path,
      bool auto_increment);
  virtual void deleteObjectStore(
      long long transaction_id,
      long long object_store_id);
  virtual void createTransaction(
      long long transaction_id,
      WebKit::WebIDBDatabaseCallbacks* callbacks,
      const WebKit::WebVector<long long>& scope,
      unsigned short mode);
  virtual void close();
  virtual void get(long long transactionId,
                   long long objectStoreId,
                   long long indexId,
                   const WebKit::WebIDBKeyRange&,
                   bool keyOnly,
                   WebKit::WebIDBCallbacks*);
  virtual void put(long long transactionId,
                   long long objectStoreId,
                   const WebKit::WebData& value,
                   const WebKit::WebIDBKey&,
                   PutMode,
                   WebKit::WebIDBCallbacks*,
                   const WebKit::WebVector<long long>& indexIds,
                   const WebKit::WebVector<WebIndexKeys>&);
  virtual void setIndexKeys(long long transactionId,
                            long long objectStoreId,
                            const WebKit::WebIDBKey&,
                            const WebKit::WebVector<long long>& indexIds,
                            const WebKit::WebVector<WebIndexKeys>&);
  virtual void setIndexesReady(long long transactionId,
                               long long objectStoreId,
                               const WebKit::WebVector<long long>& indexIds);
  virtual void openCursor(long long transactionId,
                          long long objectStoreId,
                          long long indexId,
                          const WebKit::WebIDBKeyRange&,
                          unsigned short direction,
                          bool keyOnly,
                          TaskType,
                          WebKit::WebIDBCallbacks*);
  virtual void count(long long transactionId,
                     long long objectStoreId,
                     long long indexId,
                     const WebKit::WebIDBKeyRange&,
                     WebKit::WebIDBCallbacks*);
  virtual void deleteRange(long long transactionId,
                           long long objectStoreId,
                           const WebKit::WebIDBKeyRange&,
                           WebKit::WebIDBCallbacks*);
  virtual void clear(long long transactionId,
                     long long objectStoreId,
                     WebKit::WebIDBCallbacks*);
  virtual void createIndex(long long transactionId,
                           long long objectStoreId,
                           long long indexId,
                           const WebKit::WebString& name,
                           const WebKit::WebIDBKeyPath&,
                           bool unique,
                           bool multiEntry);
  virtual void deleteIndex(long long transactionId, long
                           long objectStoreId,
                           long long indexId);
  virtual void abort(long long transaction_id);
  virtual void commit(long long transaction_id);

 private:
  int32 ipc_database_id_;
  int32 ipc_database_callbacks_id_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_PROXY_WEBIDBDATABASE_IMPL_H_
