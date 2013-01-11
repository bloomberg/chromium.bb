// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBDATABASE_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBDATABASE_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"

namespace WebKit {
class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebString;
class WebIDBTransaction;
}

namespace content {

class RendererWebIDBDatabaseImpl : public WebKit::WebIDBDatabase {
 public:
  explicit RendererWebIDBDatabaseImpl(int32 ipc_database_id);
  virtual ~RendererWebIDBDatabaseImpl();

  // WebKit::WebIDBDatabase
  virtual WebKit::WebIDBMetadata metadata() const;
  virtual void createObjectStore(
      long long transaction_id,
      long long objectstore_id,
      const WebKit::WebString& name,
      const WebKit::WebIDBKeyPath& key_path,
      bool auto_increment);
  virtual void deleteObjectStore(
      long long transaction_id,
      long long object_store_id);
  virtual WebKit::WebIDBTransaction* createTransaction(
      long long transaction_id,
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
                   WebKit::WebVector<unsigned char>* value,
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
 private:
  int32 ipc_database_id_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBDATABASE_IMPL_H_
