// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_DISPATCHER_H_

#include <map>
#include <vector>

#include "base/id_map.h"
#include "base/nullable_string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"
#include "webkit/glue/worker_task_runner.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBMsg_CallbacksSuccessCursorContinueOld_Params;
struct IndexedDBMsg_CallbacksSuccessCursorContinue_Params;
struct IndexedDBMsg_CallbacksSuccessCursorPrefetchOld_Params;
struct IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params;
struct IndexedDBMsg_CallbacksSuccessIDBCursorOld_Params;
struct IndexedDBMsg_CallbacksSuccessIDBCursor_Params;

namespace WebKit {
class WebData;
class WebFrame;
}

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class RendererWebIDBCursorImpl;
class RendererWebIDBDatabaseImpl;
class SerializedScriptValue;

CONTENT_EXPORT extern const size_t kMaxIDBValueSizeInBytes;

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class CONTENT_EXPORT IndexedDBDispatcher
    : public webkit_glue::WorkerTaskRunner::Observer {
 public:
  // Constructor made public to allow RenderThreadImpl to own a copy without
  // failing a NOTREACHED in ThreadSpecificInstance in tests that instantiate
  // two copies of RenderThreadImpl on the same thread.  Everyone else probably
  // wants to use ThreadSpecificInstance().
  IndexedDBDispatcher();
  virtual ~IndexedDBDispatcher();
  static IndexedDBDispatcher* ThreadSpecificInstance();

  // webkit_glue::WorkerTaskRunner::Observer implementation.
  virtual void OnWorkerRunLoopStopped() OVERRIDE;

  static WebKit::WebIDBMetadata ConvertMetadata(
      const IndexedDBDatabaseMetadata& idb_metadata);

  void OnMessageReceived(const IPC::Message& msg);
  static bool Send(IPC::Message* msg);

  void RequestIDBFactoryGetDatabaseNames(
      WebKit::WebIDBCallbacks* callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBFactoryOpen(
      const string16& name,
      int64 version,
      WebKit::WebIDBCallbacks* callbacks,
      WebKit::WebIDBDatabaseCallbacks* database_callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBFactoryOpen(
      const string16& name,
      int64 version,
      int64 transaction_id,
      WebKit::WebIDBCallbacks* callbacks,
      WebKit::WebIDBDatabaseCallbacks* database_callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBFactoryDeleteDatabase(
      const string16& name,
      WebKit::WebIDBCallbacks* callbacks,
      const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBCursorAdvance(
      unsigned long count,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 ipc_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBCursorContinue(
      const IndexedDBKey& key,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 ipc_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBCursorPrefetch(
      int n,
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 ipc_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBCursorPrefetchReset(int used_prefetches, int unused_prefetches,
                                     int32 ipc_cursor_id);

  void RequestIDBCursorDelete(
      WebKit::WebIDBCallbacks* callbacks_ptr,
      int32 ipc_cursor_id,
      WebKit::WebExceptionCode* ec);

  void RequestIDBDatabaseClose(
      int32 ipc_database_id);

  void RequestIDBDatabaseCreateTransaction(
      int32 ipc_database_id,
      int64 transaction_id,
      WebKit::WebIDBDatabaseCallbacks* database_callbacks_ptr,
      WebKit::WebVector<long long> object_store_ids,
      unsigned short mode);

  void RequestIDBDatabaseGet(
      int32 ipc_database_id,
      int64 transaction_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      bool key_only,
      WebKit::WebIDBCallbacks* callbacks);

  void RequestIDBDatabasePut(
      int32 ipc_database_id,
      int64 transaction_id,
      int64 object_store_id,
      const WebKit::WebData& value,
      const IndexedDBKey& key,
      WebKit::WebIDBDatabase::PutMode put_mode,
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebVector<long long>& index_ids,
      const WebKit::WebVector<WebKit::WebVector<
          WebKit::WebIDBKey> >& index_keys);

  void RequestIDBDatabaseOpenCursor(
      int32 ipc_database_id,
      int64 transaction_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      unsigned short direction,
      bool key_only,
      WebKit::WebIDBDatabase::TaskType task_type,
      WebKit::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseCount(
      int32 ipc_database_id,
      int64 transaction_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      WebKit::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseDeleteRange(
      int32 ipc_database_id,
      int64 transaction_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      WebKit::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseClear(
      int32 ipc_database_id,
      int64 transaction_id,
      int64 object_store_id,
      WebKit::WebIDBCallbacks* callbacks);

  void CursorDestroyed(int32 ipc_cursor_id);
  void DatabaseDestroyed(int32 ipc_database_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, ValueSizeTest);

  static int32 CurrentWorkerId() {
    return webkit_glue::WorkerTaskRunner::Instance()->CurrentWorkerId();
  }

  template<typename T>
  void init_params(T& params, WebKit::WebIDBCallbacks* callbacks_ptr) {
    scoped_ptr<WebKit::WebIDBCallbacks> callbacks(callbacks_ptr);
    params.ipc_thread_id = CurrentWorkerId();
    params.ipc_response_id = pending_callbacks_.Add(callbacks.release());
  }

  // IDBCallback message handlers.
  void OnSuccessIDBDatabase(int32 ipc_thread_id,
                            int32 ipc_response_id,
                            int32 ipc_object_id,
                            const IndexedDBDatabaseMetadata& idb_metadata);
  void OnSuccessIndexedDBKey(int32 ipc_thread_id,
                             int32 ipc_response_id,
                             const IndexedDBKey& key);

  void OnSuccessOpenCursorOld(
      const IndexedDBMsg_CallbacksSuccessIDBCursorOld_Params& p);
  void OnSuccessOpenCursor(
      const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p);
  void OnSuccessCursorContinueOld(
      const IndexedDBMsg_CallbacksSuccessCursorContinueOld_Params& p);
  void OnSuccessCursorContinue(
      const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p);
  void OnSuccessCursorPrefetchOld(
      const IndexedDBMsg_CallbacksSuccessCursorPrefetchOld_Params& p);
  void OnSuccessCursorPrefetch(
      const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p);
  void OnSuccessStringList(int32 ipc_thread_id,
                           int32 ipc_response_id,
                           const std::vector<string16>& value);
  void OnSuccessValue(
      int32 ipc_thread_id,
      int32 ipc_response_id,
      const std::vector<char>& value);
  void OnSuccessSerializedScriptValue(
      int32 ipc_thread_id,
      int32 ipc_response_id,
      const SerializedScriptValue& value);
  void OnSuccessSerializedScriptValueWithKey(
      int32 ipc_thread_id,
      int32 ipc_response_id,
      const SerializedScriptValue& value,
      const IndexedDBKey& primary_key,
      const IndexedDBKeyPath& key_path);
  void OnSuccessValueWithKey(
      int32 ipc_thread_id,
      int32 ipc_response_id,
      const std::vector<char>& value,
      const IndexedDBKey& primary_key,
      const IndexedDBKeyPath& key_path);
  void OnSuccessInteger(
      int32 ipc_thread_id,
      int32 ipc_response_id,
      int64 value);
  void OnSuccessUndefined(
      int32 ipc_thread_id,
      int32 ipc_response_id);
  void OnError(int32 ipc_thread_id,
               int32 ipc_response_id,
               int code,
               const string16& message);
  void OnIntBlocked(int32 ipc_thread_id, int32 ipc_response_id,
                    int64 existing_version);
  void OnUpgradeNeeded(int32 ipc_thread_id,
                       int32 ipc_response_id,
                       int32 ipc_database_id,
                       int64 old_version,
                       const IndexedDBDatabaseMetadata& metdata);
  void OnAbort(int32 ipc_thread_id,
               int32 ipc_database_id,
               int64 transaction_id,
               int code,
               const string16& message);
  void OnComplete(int32 ipc_thread_id,
                  int32 ipc_database_id,
                  int64 transaction_id);
  void OnForcedClose(int32 ipc_thread_id, int32 ipc_database_id);
  void OnVersionChange(int32 ipc_thread_id,
                       int32 ipc_database_id,
                       const string16& newVersion);
  void OnIntVersionChange(int32 ipc_thread_id,
                          int32 ipc_database_id,
                          int64 old_version,
                          int64 new_version);

  // Reset cursor prefetch caches for all cursors except exception_cursor_id.
  void ResetCursorPrefetchCaches(int32 ipc_exception_cursor_id = -1);

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<WebKit::WebIDBCallbacks, IDMapOwnPointer> pending_callbacks_;
  IDMap<WebKit::WebIDBDatabaseCallbacks, IDMapOwnPointer>
      pending_database_callbacks_;

  // Map from cursor id to RendererWebIDBCursorImpl.
  std::map<int32, RendererWebIDBCursorImpl*> cursors_;

  std::map<int32, RendererWebIDBDatabaseImpl*> databases_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
