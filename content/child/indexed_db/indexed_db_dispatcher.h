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
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBObserver.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"
#include "url/origin.h"

struct IndexedDBMsg_Observation;
struct IndexedDBMsg_ObserverChanges;

namespace blink {
struct WebIDBObservation;
}

namespace content {
class WebIDBCursorImpl;

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class CONTENT_EXPORT IndexedDBDispatcher : public WorkerThread::Observer {
 public:
  // Constructor made public to allow RenderThreadImpl to own a copy without
  // failing a NOTREACHED in ThreadSpecificInstance in tests that instantiate
  // two copies of RenderThreadImpl on the same thread.  Everyone else probably
  // wants to use ThreadSpecificInstance().
  IndexedDBDispatcher();
  ~IndexedDBDispatcher() override;

  static IndexedDBDispatcher* ThreadSpecificInstance();

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  static std::vector<blink::WebIDBObservation> ConvertObservations(
      const std::vector<IndexedDBMsg_Observation>& idb_observation);

  void OnMessageReceived(const IPC::Message& msg);

  int32_t RegisterObserver(std::unique_ptr<blink::WebIDBObserver> observer);

  // Removes observers from our local map observers_.
  void RemoveObservers(const std::vector<int32_t>& observer_ids_to_remove);

  enum { kAllCursors = -1 };

  void RegisterCursor(WebIDBCursorImpl* cursor);
  void UnregisterCursor(WebIDBCursorImpl* cursor);
  // Reset cursor prefetch caches for all cursors except exception_cursor.
  void ResetCursorPrefetchCaches(int64_t transaction_id,
                                 WebIDBCursorImpl* exception_cursor);

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

  static int32_t CurrentWorkerId() { return WorkerThread::GetCurrentId(); }

  // IDBCallback message handlers.
  void OnDatabaseChanges(int32_t ipc_thread_id,
                         const IndexedDBMsg_ObserverChanges&);

  IDMap<blink::WebIDBObserver, IDMapOwnPointer> observers_;
  std::unordered_set<WebIDBCursorImpl*> cursors_;

  // Holds pointers to the worker-thread owned state of IndexedDBCallbacksImpl
  // and IndexedDBDatabaseCallbacksImpl objects to makes sure that it is
  // destroyed on thread exit if the Mojo pipe is not yet closed. Otherwise the
  // object will leak because the thread's task runner is no longer executing
  // tasks.
  std::unordered_set<IndexedDBCallbacksImpl::InternalState*>
      mojo_owned_callback_state_;
  std::unordered_set<blink::WebIDBDatabaseCallbacks*>
      mojo_owned_database_callback_state_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
