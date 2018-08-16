// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
#define CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/indexed_db/indexed_db_callbacks_impl.h"
#include "content/renderer/indexed_db/indexed_db_database_callbacks_impl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "url/origin.h"

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

  void RegisterCursor(WebIDBCursorImpl* cursor);
  void UnregisterCursor(WebIDBCursorImpl* cursor);
  // Reset cursor prefetch caches for all cursors except exception_cursor.
  void ResetCursorPrefetchCaches(int64_t transaction_id,
                                 WebIDBCursorImpl* exception_cursor);

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);

  std::unordered_set<WebIDBCursorImpl*> cursors_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
