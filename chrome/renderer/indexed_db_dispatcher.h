// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
#define CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
#pragma once

#include "base/id_map.h"
#include "base/nullable_string16.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransactionCallbacks.h"

class IndexedDBKey;
class SerializedScriptValue;

namespace WebKit {
class WebFrame;
class WebIDBKeyRange;
class WebIDBTransaction;
}

// Handle the indexed db related communication for this entire renderer.
class IndexedDBDispatcher {
 public:
  IndexedDBDispatcher();
  ~IndexedDBDispatcher();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled.
  bool OnMessageReceived(const IPC::Message& msg);

  void RequestIDBFactoryOpen(
      const string16& name,
      const string16& description,
      WebKit::WebIDBCallbacks* callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame,
      uint64 maximum_size);

  void RequestIDBCursorUpdate(
      const SerializedScriptValue& value,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id);

  void RequestIDBCursorContinue(
      const IndexedDBKey& key,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id);

  void RequestIDBCursorRemove(
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id);

  void RequestIDBDatabaseSetVersion(
      const string16& version,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_database_id);

  void RequestIDBIndexOpenObjectCursor(
      const WebKit::WebIDBKeyRange& idb_key_range,
      unsigned short direction,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_index_id,
      const WebKit::WebIDBTransaction& transaction);

  void RequestIDBIndexOpenCursor(const WebKit::WebIDBKeyRange& idb_key_range,
                                 unsigned short direction,
                                 WebKit::WebIDBCallbacks* callbacks,
                                 int32 idb_index_id,
                                 const WebKit::WebIDBTransaction& transaction);

  void RequestIDBIndexGetObject(const IndexedDBKey& key,
                                WebKit::WebIDBCallbacks* callbacks,
                                int32 idb_index_id,
                                const WebKit::WebIDBTransaction& transaction);

  void RequestIDBIndexGet(const IndexedDBKey& key,
                          WebKit::WebIDBCallbacks* callbacks,
                          int32 idb_index_id,
                          const WebKit::WebIDBTransaction& transaction);

  void RequestIDBObjectStoreGet(const IndexedDBKey& key,
                                WebKit::WebIDBCallbacks* callbacks,
                                int32 idb_object_store_id,
                                const WebKit::WebIDBTransaction& transaction);

  void RequestIDBObjectStorePut(const SerializedScriptValue& value,
                                const IndexedDBKey& key,
                                bool add_only,
                                WebKit::WebIDBCallbacks* callbacks,
                                int32 idb_object_store_id,
                                const WebKit::WebIDBTransaction& transaction);

  void RequestIDBObjectStoreRemove(
      const IndexedDBKey& key,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id,
      const WebKit::WebIDBTransaction& transaction);

  void RequestIDBObjectStoreOpenCursor(
      const WebKit::WebIDBKeyRange& idb_key_range,
      unsigned short direction,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id,
      const WebKit::WebIDBTransaction& transaction);

  void RegisterWebIDBTransactionCallbacks(
      WebKit::WebIDBTransactionCallbacks* callbacks,
      int32 id);

  static int32 TransactionId(const WebKit::WebIDBTransaction& transaction);

 private:
  // IDBCallback message handlers.
  void OnSuccessNull(int32 response_id);
  void OnSuccessIDBDatabase(int32 response_id, int32 object_id);
  void OnSuccessIndexedDBKey(int32 response_id, const IndexedDBKey& key);
  void OnSuccessIDBObjectStore(int32 response_id, int32 object_id);
  void OnSuccessIDBTransaction(int32 response_id, int32 object_id);
  void OnSuccessIDBIndex(int32 response_id, int32 object_id);
  void OnSuccessOpenCursor(int32 response_id, int32 object_id);
  void OnSuccessSerializedScriptValue(int32 response_id,
                                      const SerializedScriptValue& value);
  void OnError(int32 response_id, int code, const string16& message);
  void OnAbort(int32 transaction_id);
  void OnComplete(int32 transaction_id);
  void OnTimeout(int32 transaction_id);

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<WebKit::WebIDBCallbacks, IDMapOwnPointer> pending_callbacks_;
  IDMap<WebKit::WebIDBTransactionCallbacks, IDMapOwnPointer>
      pending_transaction_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

#endif  // CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
