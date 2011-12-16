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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransactionCallbacks.h"
#include "webkit/glue/worker_task_runner.h"

class IndexedDBKey;
struct IndexedDBMsg_CallbacksSuccessCursorContinue_Params;
struct IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params;
struct IndexedDBMsg_CallbacksSuccessIDBCursor_Params;
class RendererWebIDBCursorImpl;

namespace IPC {
class Message;
}

namespace WebKit {
class WebFrame;
class WebIDBKeyRange;
class WebIDBTransaction;
}

namespace content {
class SerializedScriptValue;
}

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class IndexedDBDispatcher : public webkit_glue::WorkerTaskRunner::Observer {
 public:
  virtual ~IndexedDBDispatcher();
  static IndexedDBDispatcher* ThreadSpecificInstance();

  // webkit_glue::WorkerTaskRunner::Observer implementation.
  virtual void OnWorkerRunLoopStopped() OVERRIDE;

  void OnMessageReceived(const IPC::Message& msg);
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

  void RequestIDBIndexCount(
      const WebKit::WebIDBKeyRange& idb_key_range,
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

  void RequestIDBObjectStoreCount(
      const WebKit::WebIDBKeyRange& idb_key_range,
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
  IndexedDBDispatcher();
  // IDBCallback message handlers.
  void OnSuccessNull(int32 response_id);
  void OnSuccessIDBDatabase(int32 thread_id,
                            int32 response_id,
                            int32 object_id);
  void OnSuccessIndexedDBKey(int32 thread_id,
                             int32 response_id,
                             const IndexedDBKey& key);
  void OnSuccessIDBTransaction(int32 thread_id,
                               int32 response_id,
                               int32 object_id);
  void OnSuccessOpenCursor(
      const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p);
  void OnSuccessCursorContinue(
      const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p);
  void OnSuccessCursorPrefetch(
      const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p);
  void OnSuccessStringList(int32 thread_id,
                           int32 response_id,
                           const std::vector<string16>& value);
  void OnSuccessSerializedScriptValue(
      int32 thread_id,
      int32 response_id,
      const content::SerializedScriptValue& value);
  void OnError(int32 thread_id,
               int32 response_id,
               int code,
               const string16& message);
  void OnBlocked(int32 thread_id, int32 response_id);
  void OnAbort(int32 thread_id, int32 transaction_id);
  void OnComplete(int32 thread_id, int32 transaction_id);
  void OnVersionChange(int32 thread_id,
                       int32 database_id,
                       const string16& newVersion);

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
