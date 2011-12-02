// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_DISPATCHER_H_
#define CONTENT_RENDERER_INDEXED_DB_DISPATCHER_H_
#pragma once

#include <map>
#include <vector>

#include "base/id_map.h"
#include "base/nullable_string16.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransactionCallbacks.h"

class IndexedDBKey;
class RendererWebIDBCursorImpl;

namespace WebKit {
class WebFrame;
class WebIDBKeyRange;
class WebIDBTransaction;
}

namespace content {
class SerializedScriptValue;
}

// Handle the indexed db related communication for this entire renderer.
class IndexedDBDispatcher : public IPC::Channel::Listener {
 public:
  IndexedDBDispatcher();
  virtual ~IndexedDBDispatcher();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  void Send(IPC::Message* msg);

  void RequestIDBFactoryGetDatabaseNames(
      WebKit::WebIDBCallbacks* callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBFactoryOpen(
      const string16& name,
      WebKit::WebIDBCallbacks* callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBFactoryDeleteDatabase(
      const string16& name,
      WebKit::WebIDBCallbacks* callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBCursorUpdate(
      const content::SerializedScriptValue& value,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBCursorContinue(
      const IndexedDBKey& key,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBCursorPrefetch(
      int n,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBCursorPrefetchReset(int used_prefetches, int unused_prefetches,
                                     int32 idb_cursor_id);

  void RequestIDBCursorDelete(
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 idb_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBDatabaseClose(
      int32 idb_database_id);

  void RequestIDBDatabaseOpen(
      WebKit::WebIDBDatabaseCallbacks* callbacks_ptr,
      int32 idb_database_id);

  void RequestIDBDatabaseSetVersion(
      const string16& version,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_database_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBIndexOpenObjectCursor(
      const WebKit::WebIDBKeyRange& idb_key_range,
      unsigned short direction,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_index_id,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode* ec);

  void RequestIDBIndexOpenKeyCursor(
      const WebKit::WebIDBKeyRange& idb_key_range,
      unsigned short direction,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_index_id,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode* ec);

  void RequestIDBIndexGetObject(const IndexedDBKey& key,
                                WebKit::WebIDBCallbacks* callbacks,
                                int32 idb_index_id,
                                const WebKit::WebIDBTransaction& transaction,
                                 WebKit::WebExceptionCode* ec);

  void RequestIDBIndexGetKey(const IndexedDBKey& key,
                             WebKit::WebIDBCallbacks* callbacks,
                             int32 idb_index_id,
                             const WebKit::WebIDBTransaction& transaction,
                             WebKit::WebExceptionCode* ec);

  void RequestIDBObjectStoreGet(const IndexedDBKey& key,
                                WebKit::WebIDBCallbacks* callbacks,
                                int32 idb_object_store_id,
                                const WebKit::WebIDBTransaction& transaction,
                                WebKit::WebExceptionCode* ec);

  void RequestIDBObjectStorePut(const content::SerializedScriptValue& value,
                                const IndexedDBKey& key,
                                WebKit::WebIDBObjectStore::PutMode putMode,
                                WebKit::WebIDBCallbacks* callbacks,
                                int32 idb_object_store_id,
                                const WebKit::WebIDBTransaction& transaction,
                                WebKit::WebExceptionCode* ec);

  void RequestIDBObjectStoreDelete(
      const IndexedDBKey& key,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode* ec);

  void RequestIDBObjectStoreClear(
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode* ec);

  void RequestIDBObjectStoreOpenCursor(
      const WebKit::WebIDBKeyRange& idb_key_range,
      unsigned short direction,
      WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode* ec);

  void RegisterWebIDBTransactionCallbacks(
      WebKit::WebIDBTransactionCallbacks* callbacks,
      int32 id);

  void CursorDestroyed(int32 cursor_id);

  static int32 TransactionId(const WebKit::WebIDBTransaction& transaction);

 private:
  // IDBCallback message handlers.
  void OnSuccessNull(int32 response_id);
  void OnSuccessIDBDatabase(int32 response_id, int32 object_id);
  void OnSuccessIndexedDBKey(int32 response_id, const IndexedDBKey& key);
  void OnSuccessIDBTransaction(int32 response_id, int32 object_id);
  void OnSuccessOpenCursor(int32 response_id, int32 object_id,
                           const IndexedDBKey& key,
                           const IndexedDBKey& primary_key,
                           const content::SerializedScriptValue& value);
  void OnSuccessCursorContinue(int32 response_id,
                               int32 cursor_id,
                               const IndexedDBKey& key,
                               const IndexedDBKey& primary_key,
                               const content::SerializedScriptValue& value);
  void OnSuccessCursorPrefetch(
      int32 response_id,
      int32 cursor_id,
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      const std::vector<content::SerializedScriptValue>& values);
  void OnSuccessStringList(int32 response_id,
                           const std::vector<string16>& value);
  void OnSuccessSerializedScriptValue(
      int32 response_id,
      const content::SerializedScriptValue& value);
  void OnError(int32 response_id, int code, const string16& message);
  void OnBlocked(int32 response_id);
  void OnAbort(int32 transaction_id);
  void OnComplete(int32 transaction_id);
  void OnVersionChange(int32 database_id, const string16& newVersion);

  // Reset cursor prefetch caches for all cursors except exception_cursor_id.
  void ResetCursorPrefetchCaches(int32 exception_cursor_id = -1);

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<WebKit::WebIDBCallbacks, IDMapOwnPointer> pending_callbacks_;
  IDMap<WebKit::WebIDBTransactionCallbacks, IDMapOwnPointer>
      pending_transaction_callbacks_;
  IDMap<WebKit::WebIDBDatabaseCallbacks, IDMapOwnPointer>
      pending_database_callbacks_;

  // Map from cursor id to RendererWebIDBCursorImpl.
  std::map<int32, RendererWebIDBCursorImpl*> cursors_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

#endif  // CONTENT_RENDERER_INDEXED_DB_DISPATCHER_H_
