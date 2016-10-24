// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "content/child/indexed_db/indexed_db_callbacks_impl.h"
#include "content/child/indexed_db/indexed_db_database_callbacks_impl.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/public/child/worker_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBObserver.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"
#include "url/origin.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBMsg_CallbacksSuccessCursorContinue_Params;
struct IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params;
struct IndexedDBMsg_CallbacksSuccessIDBCursor_Params;
struct IndexedDBMsg_CallbacksSuccessArray_Params;
struct IndexedDBMsg_CallbacksSuccessValue_Params;
struct IndexedDBMsg_CallbacksUpgradeNeeded_Params;
struct IndexedDBMsg_Observation;
struct IndexedDBMsg_ObserverChanges;

namespace blink {
class WebData;
struct WebIDBObservation;
}

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class WebIDBCursorImpl;
class WebIDBDatabaseImpl;
class ThreadSafeSender;

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class CONTENT_EXPORT IndexedDBDispatcher : public WorkerThread::Observer {
 public:
  // Constructor made public to allow RenderThreadImpl to own a copy without
  // failing a NOTREACHED in ThreadSpecificInstance in tests that instantiate
  // two copies of RenderThreadImpl on the same thread.  Everyone else probably
  // wants to use ThreadSpecificInstance().
  explicit IndexedDBDispatcher(ThreadSafeSender* thread_safe_sender);
  ~IndexedDBDispatcher() override;

  // |thread_safe_sender| needs to be passed in because if the call leads to
  // construction it will be needed.
  static IndexedDBDispatcher* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  static std::vector<blink::WebIDBObservation> ConvertObservations(
      const std::vector<IndexedDBMsg_Observation>& idb_observation);

  void OnMessageReceived(const IPC::Message& msg);

  // This method is virtual so it can be overridden in unit tests.
  virtual bool Send(IPC::Message* msg);

  int32_t AddIDBObserver(int32_t ipc_database_id,
                         int64_t transaction_id,
                         std::unique_ptr<blink::WebIDBObserver> observer);

  //  The observer with ID's in |observer_ids_to_remove| observe the
  //  |ipc_database_id|.
  //  We remove our local references to these observer objects, and send an IPC
  //  to clean up the observers from the backend.
  void RemoveIDBObserversFromDatabase(
      int32_t ipc_database_id,
      const std::vector<int32_t>& observer_ids_to_remove);

  // Removes observers from our local map observers_ . No IPC message generated.
  void RemoveIDBObservers(const std::set<int32_t>& observer_ids_to_remove);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorAdvance(unsigned long count,
                                       blink::WebIDBCallbacks* callbacks_ptr,
                                       int32_t ipc_cursor_id,
                                       int64_t transaction_id);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorContinue(const IndexedDBKey& key,
                                        const IndexedDBKey& primary_key,
                                        blink::WebIDBCallbacks* callbacks_ptr,
                                        int32_t ipc_cursor_id,
                                        int64_t transaction_id);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorPrefetch(int n,
                                        blink::WebIDBCallbacks* callbacks_ptr,
                                        int32_t ipc_cursor_id);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorPrefetchReset(int used_prefetches,
                                             int unused_prefetches,
                                             int32_t ipc_cursor_id);

  void RequestIDBDatabaseClose(int32_t ipc_database_id);

  void NotifyIDBDatabaseVersionChangeIgnored(int32_t ipc_database_id);

  void RequestIDBDatabaseCreateTransaction(
      int32_t ipc_database_id,
      int64_t transaction_id,
      blink::WebVector<long long> object_store_ids,
      blink::WebIDBTransactionMode mode);

  void RequestIDBDatabaseGet(int32_t ipc_database_id,
                             int64_t transaction_id,
                             int64_t object_store_id,
                             int64_t index_id,
                             const IndexedDBKeyRange& key_range,
                             bool key_only,
                             blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseGetAll(int32_t ipc_database_id,
                                int64_t transaction_id,
                                int64_t object_store_id,
                                int64_t index_id,
                                const IndexedDBKeyRange& key_range,
                                bool key_only,
                                int64_t max_count,
                                blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabasePut(
      int32_t ipc_database_id,
      int64_t transaction_id,
      int64_t object_store_id,
      const blink::WebData& value,
      const blink::WebVector<blink::WebBlobInfo>& web_blob_info,
      const IndexedDBKey& key,
      blink::WebIDBPutMode put_mode,
      blink::WebIDBCallbacks* callbacks,
      const blink::WebVector<long long>& index_ids,
      const blink::WebVector<blink::WebVector<blink::WebIDBKey>>& index_keys);

  void RequestIDBDatabaseOpenCursor(int32_t ipc_database_id,
                                    int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const IndexedDBKeyRange& key_range,
                                    blink::WebIDBCursorDirection direction,
                                    bool key_only,
                                    blink::WebIDBTaskType task_type,
                                    blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseCount(int32_t ipc_database_id,
                               int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const IndexedDBKeyRange& key_range,
                               blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseDeleteRange(int32_t ipc_database_id,
                                     int64_t transaction_id,
                                     int64_t object_store_id,
                                     const IndexedDBKeyRange& key_range,
                                     blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseClear(int32_t ipc_database_id,
                               int64_t transaction_id,
                               int64_t object_store_id,
                               blink::WebIDBCallbacks* callbacks);

  virtual void CursorDestroyed(int32_t ipc_cursor_id);
  void DatabaseDestroyed(int32_t ipc_database_id);
  blink::WebIDBDatabase* RegisterDatabase(int32_t ipc_database_id);

  void RegisterMojoOwnedCallbacks(
      IndexedDBCallbacksImpl::InternalState* callback_state);
  void UnregisterMojoOwnedCallbacks(
      IndexedDBCallbacksImpl::InternalState* callback_state);
  void RegisterMojoOwnedDatabaseCallbacks(
      blink::WebIDBDatabaseCallbacks* callback_state);
  void UnregisterMojoOwnedDatabaseCallbacks(
      blink::WebIDBDatabaseCallbacks* callback_state);

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, ValueSizeTest);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, KeyAndValueSizeTest);

  enum { kAllCursors = -1 };

  static int32_t CurrentWorkerId() { return WorkerThread::GetCurrentId(); }

  template <typename T>
  void init_params(T* params, blink::WebIDBCallbacks* callbacks_ptr) {
    std::unique_ptr<blink::WebIDBCallbacks> callbacks(callbacks_ptr);
    params->ipc_thread_id = CurrentWorkerId();
    params->ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  }

  // IDBCallback message handlers.
  void OnSuccessIndexedDBKey(int32_t ipc_thread_id,
                             int32_t ipc_callbacks_id,
                             const IndexedDBKey& key);

  void OnSuccessOpenCursor(
      const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p);
  void OnSuccessCursorContinue(
      const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p);
  void OnSuccessCursorPrefetch(
      const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p);
  void OnSuccessValue(const IndexedDBMsg_CallbacksSuccessValue_Params& p);
  void OnSuccessArray(const IndexedDBMsg_CallbacksSuccessArray_Params& p);
  void OnSuccessInteger(int32_t ipc_thread_id,
                        int32_t ipc_callbacks_id,
                        int64_t value);
  void OnSuccessUndefined(int32_t ipc_thread_id, int32_t ipc_callbacks_id);
  void OnError(int32_t ipc_thread_id,
               int32_t ipc_callbacks_id,
               int code,
               const base::string16& message);
  void OnDatabaseChanges(int32_t ipc_thread_id,
                         const IndexedDBMsg_ObserverChanges&);

  // Reset cursor prefetch caches for all cursors except exception_cursor_id.
  void ResetCursorPrefetchCaches(int64_t transaction_id,
                                 int32_t ipc_exception_cursor_id);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Maximum size (in bytes) of value/key pair allowed for put requests. Any
  // requests larger than this size will be rejected.
  // Used by unit tests to exercise behavior without allocating huge chunks
  // of memory.
  size_t max_put_value_size_ = kMaxIDBMessageSizeInBytes;

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<blink::WebIDBCallbacks, IDMapOwnPointer> pending_callbacks_;
  IDMap<blink::WebIDBObserver, IDMapOwnPointer> observers_;

  // Holds pointers to the worker-thread owned state of IndexedDBCallbacksImpl
  // and IndexedDBDatabaseCallbacksImpl objects to makes sure that it is
  // destroyed on thread exit if the Mojo pipe is not yet closed. Otherwise the
  // object will leak because the thread's task runner is no longer executing
  // tasks.
  std::unordered_set<IndexedDBCallbacksImpl::InternalState*>
      mojo_owned_callback_state_;
  std::unordered_set<blink::WebIDBDatabaseCallbacks*>
      mojo_owned_database_callback_state_;

  // Maps the ipc_callback_id from an open cursor request to the request's
  // transaction_id. Used to assign the transaction_id to the WebIDBCursorImpl
  // when it is created.
  std::map<int32_t, int64_t> cursor_transaction_ids_;

  // Map from cursor id to WebIDBCursorImpl.
  std::map<int32_t, WebIDBCursorImpl*> cursors_;

  std::map<int32_t, WebIDBDatabaseImpl*> databases_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
