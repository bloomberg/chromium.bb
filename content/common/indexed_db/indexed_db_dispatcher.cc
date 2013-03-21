// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_dispatcher.h"

#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/threading/thread_local.h"
#include "content/common/child_thread.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/proxy_webidbcursor_impl.h"
#include "content/common/indexed_db/proxy_webidbdatabase_impl.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseException.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"

using WebKit::WebDOMStringList;
using WebKit::WebData;
using WebKit::WebExceptionCode;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBKey;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBMetadata;
using WebKit::WebString;
using WebKit::WebVector;
using base::ThreadLocalPointer;
using webkit_glue::WorkerTaskRunner;

namespace content {
static base::LazyInstance<ThreadLocalPointer<IndexedDBDispatcher> >::Leaky
    g_idb_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

IndexedDBDispatcher* const kHasBeenDeleted =
    reinterpret_cast<IndexedDBDispatcher*>(0x1);

int32 CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}
}  // unnamed namespace

const size_t kMaxIDBValueSizeInBytes = 64 * 1024 * 1024;

IndexedDBDispatcher::IndexedDBDispatcher() {
  g_idb_dispatcher_tls.Pointer()->Set(this);
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
  // Clear any pending callbacks - which may result in dispatch requests -
  // before marking the dispatcher as deleted.
  pending_callbacks_.Clear();
  pending_database_callbacks_.Clear();

  DCHECK(pending_callbacks_.IsEmpty());
  DCHECK(pending_database_callbacks_.IsEmpty());

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

WebIDBMetadata IndexedDBDispatcher::ConvertMetadata(
    const IndexedDBDatabaseMetadata& idb_metadata) {
  WebIDBMetadata web_metadata;
  web_metadata.id = idb_metadata.id;
  web_metadata.name = idb_metadata.name;
  web_metadata.version = idb_metadata.version;
  web_metadata.intVersion = idb_metadata.int_version;
  web_metadata.maxObjectStoreId = idb_metadata.max_object_store_id;
  web_metadata.objectStores = WebVector<WebIDBMetadata::ObjectStore>(
      idb_metadata.object_stores.size());

  for (size_t i = 0; i < idb_metadata.object_stores.size(); ++i) {
    const IndexedDBObjectStoreMetadata& idb_store_metadata =
        idb_metadata.object_stores[i];
    WebIDBMetadata::ObjectStore& web_store_metadata =
        web_metadata.objectStores[i];

    web_store_metadata.id = idb_store_metadata.id;
    web_store_metadata.name = idb_store_metadata.name;
    web_store_metadata.keyPath = idb_store_metadata.keyPath;
    web_store_metadata.autoIncrement = idb_store_metadata.autoIncrement;
    web_store_metadata.maxIndexId = idb_store_metadata.max_index_id;
    web_store_metadata.indexes = WebVector<WebIDBMetadata::Index>(
        idb_store_metadata.indexes.size());

    for (size_t j = 0; j < idb_store_metadata.indexes.size(); ++j) {
      const IndexedDBIndexMetadata& idb_index_metadata =
          idb_store_metadata.indexes[j];
      WebIDBMetadata::Index& web_index_metadata =
          web_store_metadata.indexes[j];

      web_index_metadata.id = idb_index_metadata.id;
      web_index_metadata.name = idb_index_metadata.name;
      web_index_metadata.keyPath = idb_index_metadata.keyPath;
      web_index_metadata.unique = idb_index_metadata.unique;
      web_index_metadata.multiEntry = idb_index_metadata.multiEntry;
    }
  }

  return web_metadata;
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
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessStringList,
                        OnSuccessStringList)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessValue,
                        OnSuccessValue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessValueWithKey,
                        OnSuccessValueWithKey)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessInteger,
                        OnSuccessInteger)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessUndefined,
                        OnSuccessUndefined)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksError, OnError)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksIntBlocked, OnIntBlocked)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksUpgradeNeeded, OnUpgradeNeeded)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksForcedClose,
                        OnForcedClose)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksIntVersionChange,
                        OnIntVersionChange)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksComplete, OnComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // If a message gets here, IndexedDBMessageFilter already determined that it
  // is an IndexedDB message.
  DCHECK(handled) << "Didn't handle a message defined at line "
                  << IPC_MESSAGE_ID_LINE(msg.type());
}

bool IndexedDBDispatcher::Send(IPC::Message* msg) {
  if (CurrentWorkerId()) {
    scoped_refptr<IPC::SyncMessageFilter> filter(
        ChildThread::current()->sync_message_filter());
    return filter->Send(msg);
  }
  return ChildThread::current()->Send(msg);
}

void IndexedDBDispatcher::RequestIDBCursorAdvance(
    unsigned long count,
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id,
    WebExceptionCode* ec) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(ipc_cursor_id);

  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorAdvance(ipc_cursor_id, CurrentWorkerId(),
                                          ipc_response_id, count));
}

void IndexedDBDispatcher::RequestIDBCursorContinue(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id,
    WebExceptionCode* ec) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(ipc_cursor_id);

  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_response_id = pending_callbacks_.Add(callbacks.release());
  Send(
      new IndexedDBHostMsg_CursorContinue(ipc_cursor_id, CurrentWorkerId(),
                                          ipc_response_id, key));
}

void IndexedDBDispatcher::RequestIDBCursorPrefetch(
    int n,
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorPrefetch(ipc_cursor_id, CurrentWorkerId(),
                                           ipc_response_id, n));
}

void IndexedDBDispatcher::RequestIDBCursorPrefetchReset(
    int used_prefetches, int unused_prefetches, int32 ipc_cursor_id) {
  Send(new IndexedDBHostMsg_CursorPrefetchReset(ipc_cursor_id,
                                                used_prefetches,
                                                unused_prefetches));
}

void IndexedDBDispatcher::RequestIDBCursorDelete(
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id,
    WebExceptionCode* ec) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorDelete(ipc_cursor_id, CurrentWorkerId(),
                                         ipc_response_id));
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name,
    int64 version,
    WebIDBCallbacks* callbacks_ptr,
    WebIDBDatabaseCallbacks* database_callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  scoped_ptr<WebIDBDatabaseCallbacks>
      database_callbacks(database_callbacks_ptr);

  if (!CurrentWorkerId() &&
      !ChildThread::current()->IsWebFrameValid(web_frame))
    return;

  IndexedDBHostMsg_FactoryOpen_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_response_id = pending_callbacks_.Add(callbacks.release());
  params.ipc_database_response_id = pending_database_callbacks_.Add(
      database_callbacks.release());
  params.origin = origin;
  params.name = name;
  params.transaction_id = 0;
  params.version = version;
  Send(new IndexedDBHostMsg_FactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name,
    int64 version,
    int64 transaction_id,
    WebIDBCallbacks* callbacks_ptr,
    WebIDBDatabaseCallbacks* database_callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  ResetCursorPrefetchCaches();
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  scoped_ptr<WebIDBDatabaseCallbacks>
      database_callbacks(database_callbacks_ptr);

  if (!CurrentWorkerId() &&
      !ChildThread::current()->IsWebFrameValid(web_frame))
    return;

  IndexedDBHostMsg_FactoryOpen_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_response_id = pending_callbacks_.Add(callbacks.release());
  params.ipc_database_response_id = pending_database_callbacks_.Add(
      database_callbacks.release());
  params.origin = origin;
  params.name = name;
  params.transaction_id = transaction_id;
  params.version = version;
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
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_response_id = pending_callbacks_.Add(callbacks.release());
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
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  params.name = name;
  Send(new IndexedDBHostMsg_FactoryDeleteDatabase(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseClose(int32 ipc_database_id) {
  ResetCursorPrefetchCaches();
  Send(new IndexedDBHostMsg_DatabaseClose(ipc_database_id));
  // There won't be pending database callbacks if the transaction was aborted in
  // the initial upgradeneeded event handler.
  if (pending_database_callbacks_.Lookup(ipc_database_id))
    pending_database_callbacks_.Remove(ipc_database_id);
}

void IndexedDBDispatcher::RequestIDBDatabaseCreateTransaction(
    int32 ipc_database_id,
    int64 transaction_id,
    WebIDBDatabaseCallbacks* database_callbacks_ptr,
    WebKit::WebVector<long long> object_store_ids,
    unsigned short mode) {
  scoped_ptr<WebIDBDatabaseCallbacks>
      database_callbacks(database_callbacks_ptr);
  IndexedDBHostMsg_DatabaseCreateTransaction_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.ipc_database_response_id = pending_database_callbacks_.Add(
      database_callbacks.release());
  params.object_store_ids.assign(object_store_ids.data(),
                                 object_store_ids.data() +
                                 object_store_ids.size());
  params.mode = mode;

  Send(new IndexedDBHostMsg_DatabaseCreateTransaction(params));
}


void IndexedDBDispatcher::RequestIDBDatabaseGet(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches();
  IndexedDBHostMsg_DatabaseGet_Params params;
  init_params(params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.key_range = key_range;
  params.key_only = key_only;
  Send(new IndexedDBHostMsg_DatabaseGet(params));
}


void IndexedDBDispatcher::RequestIDBDatabasePut(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    const WebData& value,
    const IndexedDBKey& key,
    WebIDBDatabase::PutMode put_mode,
    WebIDBCallbacks* callbacks,
    const WebVector<long long>& index_ids,
    const WebVector<WebKit::WebVector<
      WebIDBKey> >& index_keys) {

  if (value.size() > kMaxIDBValueSizeInBytes) {
    callbacks->onError(WebIDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        WebString::fromUTF8(
            base::StringPrintf(
                "The serialized value is too large"
                " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
                value.size(), kMaxIDBValueSizeInBytes).c_str())));
    return;
  }

  ResetCursorPrefetchCaches();
  IndexedDBHostMsg_DatabasePut_Params params;
  init_params(params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;

  params.value.assign(value.data(), value.data() + value.size());
  params.key = key;
  params.put_mode = put_mode;

  COMPILE_ASSERT(sizeof(params.index_ids[0]) ==
                 sizeof(index_ids[0]), Cant_copy);
  params.index_ids.assign(index_ids.data(),
                          index_ids.data() + index_ids.size());

  params.index_keys.resize(index_keys.size());
  for (size_t i = 0; i < index_keys.size(); ++i) {
      params.index_keys[i].resize(index_keys[i].size());
      for (size_t j = 0; j < index_keys[i].size(); ++j) {
          params.index_keys[i][j] = IndexedDBKey(index_keys[i][j]);
      }
  }
  Send(new IndexedDBHostMsg_DatabasePut(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseOpenCursor(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    unsigned short direction,
    bool key_only,
    WebKit::WebIDBDatabase::TaskType task_type,
    WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches();
  IndexedDBHostMsg_DatabaseOpenCursor_Params params;
  init_params(params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.key_range = IndexedDBKeyRange(key_range);
  params.direction = direction;
  params.key_only = key_only;
  params.task_type = task_type;
  Send(new IndexedDBHostMsg_DatabaseOpenCursor(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseCount(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    WebKit::WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches();
  IndexedDBHostMsg_DatabaseCount_Params params;
  init_params(params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.key_range = IndexedDBKeyRange(key_range);
  Send(new IndexedDBHostMsg_DatabaseCount(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseDeleteRange(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    const IndexedDBKeyRange& key_range,
    WebKit::WebIDBCallbacks* callbacks)
{
  ResetCursorPrefetchCaches();
  IndexedDBHostMsg_DatabaseDeleteRange_Params params;
  init_params(params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.key_range = key_range;
  Send(new IndexedDBHostMsg_DatabaseDeleteRange(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseClear(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    WebKit::WebIDBCallbacks* callbacks_ptr) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  int32 ipc_response_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_DatabaseClear(
      CurrentWorkerId(), ipc_response_id, ipc_database_id,
      transaction_id, object_store_id));
}

void IndexedDBDispatcher::CursorDestroyed(int32 ipc_cursor_id) {
  cursors_.erase(ipc_cursor_id);
}

void IndexedDBDispatcher::DatabaseDestroyed(int32 ipc_database_id) {
  DCHECK_EQ(databases_.count(ipc_database_id), 1u);
  databases_.erase(ipc_database_id);
}

void IndexedDBDispatcher::OnSuccessIDBDatabase(
    int32 ipc_thread_id,
    int32 ipc_response_id,
    int32 ipc_object_id,
    const IndexedDBDatabaseMetadata& idb_metadata) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  WebIDBMetadata metadata(ConvertMetadata(idb_metadata));
  // If an upgrade was performed, count will be non-zero.
  if (!databases_.count(ipc_object_id))
    databases_[ipc_object_id] = new RendererWebIDBDatabaseImpl(ipc_object_id);
  DCHECK_EQ(databases_.count(ipc_object_id), 1u);
  callbacks->onSuccess(databases_[ipc_object_id], metadata);
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessIndexedDBKey(
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const IndexedDBKey& key) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(WebIDBKey(key));
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessStringList(
    int32 ipc_thread_id, int32 ipc_response_id,
    const std::vector<string16>& value) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  WebDOMStringList string_list;
  for (std::vector<string16>::const_iterator it = value.begin();
       it != value.end(); ++it)
      string_list.append(*it);
  callbacks->onSuccess(string_list);
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessValue(
    int32 ipc_thread_id, int32 ipc_response_id,
    const std::vector<char>& value) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  WebData web_value;
  if (value.size())
      web_value.assign(&value.front(), value.size());
  callbacks->onSuccess(web_value);
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessValueWithKey(
    int32 ipc_thread_id, int32 ipc_response_id,
    const std::vector<char>& value,
    const IndexedDBKey& primary_key,
    const IndexedDBKeyPath& key_path) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  WebData web_value;
  if (value.size())
      web_value.assign(&value.front(), value.size());
  callbacks->onSuccess(web_value,
                       primary_key, key_path);
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessInteger(
    int32 ipc_thread_id, int32 ipc_response_id, int64 value) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessUndefined(
    int32 ipc_thread_id, int32 ipc_response_id) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess();
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessOpenCursor(
    const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32 ipc_response_id = p.ipc_response_id;
  int32 ipc_object_id = p.ipc_cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;
  WebData web_value;
  if (p.value.size())
      web_value.assign(&p.value.front(), p.value.size());

  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;

  RendererWebIDBCursorImpl* cursor =
          new RendererWebIDBCursorImpl(ipc_object_id);
  cursors_[ipc_object_id] = cursor;
  callbacks->onSuccess(cursor, key, primary_key, web_value);

  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessCursorContinue(
    const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32 ipc_response_id = p.ipc_response_id;
  int32 ipc_cursor_id = p.ipc_cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;
  const std::vector<char>& value = p.value;

  RendererWebIDBCursorImpl* cursor = cursors_[ipc_cursor_id];
  DCHECK(cursor);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;

  WebData web_value;
  if (value.size())
      web_value.assign(&value.front(), value.size());
  callbacks->onSuccess(key, primary_key, web_value);

  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnSuccessCursorPrefetch(
    const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32 ipc_response_id = p.ipc_response_id;
  int32 ipc_cursor_id = p.ipc_cursor_id;
  const std::vector<IndexedDBKey>& keys = p.keys;
  const std::vector<IndexedDBKey>& primary_keys = p.primary_keys;
  std::vector<WebData> values(p.values.size());
  for (size_t i = 0; i < p.values.size(); ++i) {
      if (p.values[i].size())
          values[i].assign(&p.values[i].front(), p.values[i].size());
  }
  RendererWebIDBCursorImpl* cursor = cursors_[ipc_cursor_id];
  DCHECK(cursor);
  cursor->SetPrefetchData(keys, primary_keys, values);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  DCHECK(callbacks);
  cursor->CachedContinue(callbacks);
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnIntBlocked(int32 ipc_thread_id,
                                       int32 ipc_response_id,
                                       int64 existing_version) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  DCHECK(callbacks);
  callbacks->onBlocked(existing_version);
}

void IndexedDBDispatcher::OnUpgradeNeeded(
    int32 ipc_thread_id,
    int32 ipc_response_id,
    int32 ipc_database_id,
    int64 old_version,
    const IndexedDBDatabaseMetadata& idb_metadata) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  DCHECK(callbacks);
  WebIDBMetadata metadata(ConvertMetadata(idb_metadata));
  DCHECK(!databases_.count(ipc_database_id));
  databases_[ipc_database_id] = new RendererWebIDBDatabaseImpl(ipc_database_id);
  callbacks->onUpgradeNeeded(
      old_version,
      databases_[ipc_database_id],
      metadata);
}

void IndexedDBDispatcher::OnError(int32 ipc_thread_id, int32 ipc_response_id,
                                  int code,
                                  const string16& message) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_response_id);
  if (!callbacks)
    return;
  callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(ipc_response_id);
}

void IndexedDBDispatcher::OnAbort(int32 ipc_thread_id, int32 ipc_database_id,
                                  int64 transaction_id,
                                  int code, const string16& message) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_id);
  if (!callbacks)
    return;
  callbacks->onAbort(transaction_id, WebIDBDatabaseError(code, message));
}

void IndexedDBDispatcher::OnComplete(int32 ipc_thread_id,
                                     int32 ipc_database_id,
                                     int64 transaction_id) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_id);
  if (!callbacks)
    return;
  callbacks->onComplete(transaction_id);
}

void IndexedDBDispatcher::OnForcedClose(int32 ipc_thread_id,
                                        int32 ipc_database_id) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_id);
  if (!callbacks)
    return;
  callbacks->onForcedClose();
}

void IndexedDBDispatcher::OnIntVersionChange(int32 ipc_thread_id,
                                             int32 ipc_database_id,
                                             int64 old_version,
                                             int64 new_version) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_id);
  // callbacks would be NULL if a versionchange event is received after close
  // has been called.
  if (!callbacks)
    return;
  callbacks->onVersionChange(old_version, new_version);
}

void IndexedDBDispatcher::ResetCursorPrefetchCaches(
    int32 ipc_exception_cursor_id) {
  typedef std::map<int32, RendererWebIDBCursorImpl*>::iterator Iterator;
  for (Iterator i = cursors_.begin(); i != cursors_.end(); ++i) {
    if (i->first == ipc_exception_cursor_id)
      continue;
    i->second->ResetPrefetchCache();
  }
}

}  // namespace content
