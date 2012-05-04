// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_dispatcher.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/common/child_thread.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/proxy_webidbcursor_impl.h"
#include "content/common/indexed_db/proxy_webidbdatabase_impl.h"
#include "content/common/indexed_db/proxy_webidbindex_impl.h"
#include "content/common/indexed_db/proxy_webidbobjectstore_impl.h"
#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseException.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"

using base::ThreadLocalPointer;
using content::IndexedDBKey;
using content::IndexedDBKeyRange;
using content::SerializedScriptValue;
using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBTransaction;
using WebKit::WebIDBTransactionCallbacks;
using webkit_glue::WorkerTaskRunner;

static base::LazyInstance<ThreadLocalPointer<IndexedDBDispatcher> >::Leaky
    g_idb_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

IndexedDBDispatcher* const kHasBeenDeleted =
    reinterpret_cast<IndexedDBDispatcher*>(0x1);

int32 CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

} // unnamed namespace

const size_t kMaxIDBValueSizeInBytes = 64 * 1024 * 1024;

IndexedDBDispatcher::IndexedDBDispatcher() {
  g_idb_dispatcher_tls.Pointer()->Set(this);
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
  g_idb_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

IndexedDBDispatcher* IndexedDBDispatcher::ThreadSpecificInstance() {
  if (g_idb_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS IndexedDBDispatcher.";
    g_idb_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_idb_dispatcher_tls.Pointer()->Get())
    return g_idb_dispatcher_tls.Pointer()->Get();

  IndexedDBDispatcher* dispatcher = new IndexedDBDispatcher;
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    webkit_glue::WorkerTaskRunner::Instance()->AddStopObserver(dispatcher);
  return dispatcher;
}

void IndexedDBDispatcher::OnWorkerRunLoopStopped() {
  delete this;
}

void IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBCursor,
                        OnSuccessOpenCursor)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorAdvance,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorContinue,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorPrefetch,
                        OnSuccessCursorPrefetch)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                        OnSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                        OnSuccessIndexedDBKey)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBTransaction,
                        OnSuccessIDBTransaction)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessStringList,
                        OnSuccessStringList)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessSerializedScriptValue,
                        OnSuccessSerializedScriptValue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksError, OnError)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksBlocked, OnBlocked)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksComplete, OnComplete)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksVersionChange,
                        OnVersionChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // If a message gets here, IndexedDBMessageFilter already determined that it
  // is an IndexedDB message.
  DCHECK(handled);
}

bool IndexedDBDispatcher::Send(IPC::Message* msg) {
  if (CurrentWorkerId()) {
    scoped_refptr<IPC::SyncMessageFilter> filter(
        ChildThread::current()->sync_message_filter());
    return filter->Send(msg);
  }
  return ChildThread::current()->Send(msg);
}

void IndexedDBDispatcher::RequestIDBCursorUpdate(
    const SerializedScriptValue& value,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  if (!value.is_null() &&
      (value.data().length() * sizeof(char16)) > kMaxIDBValueSizeInBytes) {
    *ec = WebKit::WebIDBDatabaseExceptionDataError;
    return;
  }
  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(
      new IndexedDBHostMsg_CursorUpdate(idb_cursor_id, CurrentWorkerId(),
                                        response_id, value, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorAdvance(
    unsigned long count,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(idb_cursor_id);

  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorAdvance(idb_cursor_id, CurrentWorkerId(),
                                          response_id, count, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorContinue(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(idb_cursor_id);

  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(
      new IndexedDBHostMsg_CursorContinue(idb_cursor_id, CurrentWorkerId(),
                                          response_id, key, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorPrefetch(
    int n,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorPrefetch(idb_cursor_id, CurrentWorkerId(),
                                           response_id, n, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorPrefetchReset(
    int used_prefetches, int unused_prefetches, int32 idb_cursor_id) {
  Send(new IndexedDBHostMsg_CursorPrefetchReset(idb_cursor_id,
                                                used_prefetches,
                                                unused_prefetches));
}

void IndexedDBDispatcher::RequestIDBCursorDelete(
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorDelete(idb_cursor_id, CurrentWorkerId(),
                                         response_id, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name,
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!CurrentWorkerId() &&
      !ChildThread::current()->IsWebFrameValid(web_frame))
    return;

  IndexedDBHostMsg_FactoryOpen_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  params.name = name;
  Send(new IndexedDBHostMsg_FactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBFactoryGetDatabaseNames(
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!CurrentWorkerId() &&
      !ChildThread::current()->IsWebFrameValid(web_frame))
    return;

  IndexedDBHostMsg_FactoryGetDatabaseNames_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  Send(new IndexedDBHostMsg_FactoryGetDatabaseNames(params));
}

void IndexedDBDispatcher::RequestIDBFactoryDeleteDatabase(
    const string16& name,
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!CurrentWorkerId() &&
      !ChildThread::current()->IsWebFrameValid(web_frame))
    return;

  IndexedDBHostMsg_FactoryDeleteDatabase_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  params.name = name;
  Send(new IndexedDBHostMsg_FactoryDeleteDatabase(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseClose(int32 idb_database_id) {
  ResetCursorPrefetchCaches();
  Send(new IndexedDBHostMsg_DatabaseClose(idb_database_id));
  pending_database_callbacks_.Remove(idb_database_id);
}

void IndexedDBDispatcher::RequestIDBDatabaseOpen(
      WebIDBDatabaseCallbacks* callbacks_ptr,
      int32 idb_database_id) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBDatabaseCallbacks> callbacks(callbacks_ptr);

  DCHECK(!pending_database_callbacks_.Lookup(idb_database_id));
  pending_database_callbacks_.AddWithID(callbacks.release(), idb_database_id);
  Send(new IndexedDBHostMsg_DatabaseOpen(idb_database_id, CurrentWorkerId(),
                                         idb_database_id));
}

void IndexedDBDispatcher::RequestIDBDatabaseSetVersion(
    const string16& version,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_database_id,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_DatabaseSetVersion(idb_database_id,
                                               CurrentWorkerId(),
                                               response_id, version, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBIndexOpenObjectCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_IndexOpenCursor_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.key_range = IndexedDBKeyRange(idb_key_range);
  params.direction = direction;
  params.idb_index_id = idb_index_id;
  params.transaction_id = TransactionId(transaction);
  Send(new IndexedDBHostMsg_IndexOpenObjectCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBIndexOpenKeyCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_IndexOpenCursor_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.key_range = IndexedDBKeyRange(idb_key_range);
  params.direction = direction;
  params.idb_index_id = idb_index_id;
  params.transaction_id = TransactionId(transaction);
  Send(new IndexedDBHostMsg_IndexOpenKeyCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBIndexCount(
    const WebIDBKeyRange& idb_key_range,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_IndexCount_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.key_range = IndexedDBKeyRange(idb_key_range);
  params.idb_index_id = idb_index_id;
  params.transaction_id = TransactionId(transaction);
  Send(new IndexedDBHostMsg_IndexCount(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBIndexGetObject(
    const IndexedDBKeyRange& key_range,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_IndexGetObject(
           idb_index_id, CurrentWorkerId(),
           response_id, key_range,
           TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBIndexGetKey(
    const IndexedDBKeyRange& key_range,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_IndexGetKey(
           idb_index_id, CurrentWorkerId(), response_id, key_range,
           TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreGet(
    const IndexedDBKeyRange& key_range,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_ObjectStoreGet(
           idb_object_store_id, CurrentWorkerId(), response_id,
           key_range, TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStorePut(
    const SerializedScriptValue& value,
    const IndexedDBKey& key,
    WebKit::WebIDBObjectStore::PutMode put_mode,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  if (!value.is_null() &&
      (value.data().length() * sizeof(char16)) > kMaxIDBValueSizeInBytes) {
    *ec = WebKit::WebIDBDatabaseExceptionDataError;
    return;
  }
  IndexedDBHostMsg_ObjectStorePut_Params params;
  params.thread_id = CurrentWorkerId();
  params.idb_object_store_id = idb_object_store_id;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.serialized_value = value;
  params.key = key;
  params.put_mode = put_mode;
  params.transaction_id = TransactionId(transaction);
  Send(new IndexedDBHostMsg_ObjectStorePut(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreDelete(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_ObjectStoreDelete(
      idb_object_store_id, CurrentWorkerId(), response_id, key,
      TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreDeleteRange(
    const IndexedDBKeyRange& key_range,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_ObjectStoreDeleteRange(
      idb_object_store_id, CurrentWorkerId(), response_id, key_range,
      TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreClear(
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_ObjectStoreClear(
      idb_object_store_id, CurrentWorkerId(), response_id,
      TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreOpenCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_ObjectStoreOpenCursor_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.key_range = IndexedDBKeyRange(idb_key_range);
  params.direction = direction;
  params.idb_object_store_id = idb_object_store_id;
  params.transaction_id = TransactionId(transaction);
  Send(new IndexedDBHostMsg_ObjectStoreOpenCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreCount(
    const WebIDBKeyRange& idb_key_range,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_ObjectStoreCount_Params params;
  params.thread_id = CurrentWorkerId();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.key_range = IndexedDBKeyRange(idb_key_range);
  params.idb_object_store_id = idb_object_store_id;
  params.transaction_id = TransactionId(transaction);
  Send(new IndexedDBHostMsg_ObjectStoreCount(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RegisterWebIDBTransactionCallbacks(
    WebIDBTransactionCallbacks* callbacks,
    int32 id) {
  pending_transaction_callbacks_.AddWithID(callbacks, id);
}

void IndexedDBDispatcher::CursorDestroyed(int32 cursor_id) {
  cursors_.erase(cursor_id);
}

int32 IndexedDBDispatcher::TransactionId(
    const WebIDBTransaction& transaction) {
  const RendererWebIDBTransactionImpl* impl =
      static_cast<const RendererWebIDBTransactionImpl*>(&transaction);
  return impl->id();
}

void IndexedDBDispatcher::OnSuccessIDBDatabase(int32 thread_id,
                                               int32 response_id,
                                               int32 object_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(new RendererWebIDBDatabaseImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIndexedDBKey(
    int32 thread_id,
    int32 response_id,
    const IndexedDBKey& key) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(key);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBTransaction(int32 thread_id,
                                                  int32 response_id,
                                                  int32 object_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(new RendererWebIDBTransactionImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessStringList(
    int32 thread_id, int32 response_id, const std::vector<string16>& value) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  WebDOMStringList string_list;
  for (std::vector<string16>::const_iterator it = value.begin();
       it != value.end(); ++it)
      string_list.append(*it);
  callbacks->onSuccess(string_list);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessSerializedScriptValue(
    int32 thread_id, int32 response_id,
    const SerializedScriptValue& value) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessOpenCursor(
    const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p) {
  DCHECK_EQ(p.thread_id, CurrentWorkerId());
  int32 response_id = p.response_id;
  int32 object_id = p.cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;
  const SerializedScriptValue& value = p.serialized_value;

  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;

  RendererWebIDBCursorImpl* cursor = new RendererWebIDBCursorImpl(object_id);
  cursors_[object_id] = cursor;
  cursor->SetKeyAndValue(key, primary_key, value);
  callbacks->onSuccess(cursor);

  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessCursorContinue(
    const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p) {
  DCHECK_EQ(p.thread_id, CurrentWorkerId());
  int32 response_id = p.response_id;
  int32 cursor_id = p.cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;
  const SerializedScriptValue& value = p.serialized_value;

  RendererWebIDBCursorImpl* cursor = cursors_[cursor_id];
  DCHECK(cursor);
  cursor->SetKeyAndValue(key, primary_key, value);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccessWithContinuation();

  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessCursorPrefetch(
    const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p) {
  DCHECK_EQ(p.thread_id, CurrentWorkerId());
  int32 response_id = p.response_id;
  int32 cursor_id = p.cursor_id;
  const std::vector<IndexedDBKey>& keys = p.keys;
  const std::vector<IndexedDBKey>& primary_keys = p.primary_keys;
  const std::vector<SerializedScriptValue>& values = p.values;
  RendererWebIDBCursorImpl* cursor = cursors_[cursor_id];
  DCHECK(cursor);
  cursor->SetPrefetchData(keys, primary_keys, values);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  DCHECK(callbacks);
  cursor->CachedContinue(callbacks);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnBlocked(int32 thread_id, int32 response_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  DCHECK(callbacks);
  callbacks->onBlocked();
}

void IndexedDBDispatcher::OnError(int32 thread_id, int32 response_id, int code,
                                  const string16& message) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnAbort(int32 thread_id, int32 transaction_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  if (!callbacks)
    return;
  callbacks->onAbort();
  pending_transaction_callbacks_.Remove(transaction_id);
}

void IndexedDBDispatcher::OnComplete(int32 thread_id, int32 transaction_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  if (!callbacks)
    return;
  callbacks->onComplete();
  pending_transaction_callbacks_.Remove(transaction_id);
}

void IndexedDBDispatcher::OnVersionChange(int32 thread_id,
                                          int32 database_id,
                                          const string16& newVersion) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(database_id);
  // callbacks would be NULL if a versionchange event is received after close
  // has been called.
  if (!callbacks)
    return;
  callbacks->onVersionChange(newVersion);
}

void IndexedDBDispatcher::ResetCursorPrefetchCaches(int32 exception_cursor_id) {
  typedef std::map<int32, RendererWebIDBCursorImpl*>::iterator Iterator;
  for (Iterator i = cursors_.begin(); i != cursors_.end(); ++i) {
    if (i->first == exception_cursor_id)
      continue;
    i->second->ResetPrefetchCache();
  }
}
