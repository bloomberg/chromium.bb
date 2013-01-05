// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/in_process_webkit/indexed_db_callbacks.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/in_process_webkit/indexed_db_database_callbacks.h"
#include "content/browser/in_process_webkit/indexed_db_transaction_callbacks.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBIndex.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBMetadata.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/database/database_util.h"

using webkit_database::DatabaseUtil;
using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBCursor;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBIndex;
using WebKit::WebIDBKey;
using WebKit::WebIDBMetadata;
using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransaction;
using WebKit::WebSecurityOrigin;
using WebKit::WebSerializedScriptValue;
using WebKit::WebVector;

namespace content {
namespace {

template <class T>
void DeleteOnWebKitThread(T* obj) {
  if (!BrowserThread::DeleteSoon(BrowserThread::WEBKIT_DEPRECATED,
                                 FROM_HERE, obj))
    delete obj;
}
}

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int ipc_process_id, IndexedDBContextImpl* indexed_db_context)
    : indexed_db_context_(indexed_db_context),
      ALLOW_THIS_IN_INITIALIZER_LIST(database_dispatcher_host_(
          new DatabaseDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(index_dispatcher_host_(
          new IndexDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(object_store_dispatcher_host_(
          new ObjectStoreDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(cursor_dispatcher_host_(
          new CursorDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(transaction_dispatcher_host_(
          new TransactionDispatcherHost(this))),
      ipc_process_id_(ipc_process_id) {
  DCHECK(indexed_db_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
}

void IndexedDBDispatcherHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  bool success = BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(&IndexedDBDispatcherHost::ResetDispatcherHosts, this));

  if (!success)
    ResetDispatcherHosts();
}

void IndexedDBDispatcherHost::ResetDispatcherHosts() {
  // It is important that the various *_dispatcher_host_ members are reset
  // on the WebKit thread, since there might be incoming messages on that
  // thread, and we must not reset the dispatcher hosts until after those
  // messages are processed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED) ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));

  database_dispatcher_host_.reset();
  index_dispatcher_host_.reset();
  object_store_dispatcher_host_.reset();
  cursor_dispatcher_host_.reset();
  transaction_dispatcher_host_.reset();
}

void IndexedDBDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == IndexedDBMsgStart)
    *thread = BrowserThread::WEBKIT_DEPRECATED;
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  if (IPC_MESSAGE_CLASS(message) != IndexedDBMsgStart)
    return false;

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));

  bool handled =
      database_dispatcher_host_->OnMessageReceived(message, message_was_ok) ||
      index_dispatcher_host_->OnMessageReceived(message, message_was_ok) ||
      object_store_dispatcher_host_->OnMessageReceived(
          message, message_was_ok) ||
      cursor_dispatcher_host_->OnMessageReceived(message, message_was_ok) ||
      transaction_dispatcher_host_->OnMessageReceived(message, message_was_ok);

  if (!handled) {
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost, message, *message_was_ok)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryGetDatabaseNames,
                          OnIDBFactoryGetDatabaseNames)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryOpen, OnIDBFactoryOpen)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryOpenOld, OnIDBFactoryOpenOld)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryDeleteDatabase,
                          OnIDBFactoryDeleteDatabase)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }
  return handled;
}

int32 IndexedDBDispatcherHost::Add(WebIDBCursor* idb_cursor) {
  if (!cursor_dispatcher_host_.get()) {
    delete idb_cursor;
    return 0;
  }
  return cursor_dispatcher_host_->map_.Add(idb_cursor);
}

int32 IndexedDBDispatcherHost::Add(WebIDBDatabase* idb_database,
                                   int32 ipc_thread_id,
                                   const GURL& origin_url) {
  if (!database_dispatcher_host_.get()) {
    delete idb_database;
    return 0;
  }
  int32 ipc_database_id = database_dispatcher_host_->map_.Add(idb_database);
  Context()->ConnectionOpened(origin_url, idb_database);
  database_dispatcher_host_->database_url_map_[ipc_database_id] = origin_url;
  return ipc_database_id;
}

int32 IndexedDBDispatcherHost::Add(WebIDBIndex* idb_index) {
  if (!index_dispatcher_host_.get())  {
    delete idb_index;
    return 0;
  }
  if (!idb_index)
    return 0;
  return index_dispatcher_host_->map_.Add(idb_index);
}

int32 IndexedDBDispatcherHost::Add(WebIDBObjectStore* idb_object_store) {
  if (!object_store_dispatcher_host_.get()) {
    delete idb_object_store;
    return 0;
  }
  if (!idb_object_store)
    return 0;
  return object_store_dispatcher_host_->map_.Add(idb_object_store);
}

int32 IndexedDBDispatcherHost::Add(WebIDBTransaction* idb_transaction,
                                   int32 ipc_thread_id,
                                   const GURL& url) {
  if (!transaction_dispatcher_host_.get()) {
    delete idb_transaction;
    return 0;
  }
  int32 id = transaction_dispatcher_host_->map_.Add(idb_transaction);
  idb_transaction->setCallbacks(
      new IndexedDBTransactionCallbacks(this, ipc_thread_id, id));
  transaction_dispatcher_host_->transaction_url_map_[id] = url;
  return id;
}

int64 IndexedDBDispatcherHost::HostTransactionId(int64 transaction_id) {
  // Inject the renderer process id into the transaction id, to
  // uniquely identify this transaction, and effectively bind it to
  // the renderer that initiated it. The lower 32 bits of
  // transaction_id are guaranteed to be unique within that renderer.
  base::ProcessId pid = base::GetProcId(peer_handle());
  DCHECK(!(transaction_id >> 32)) << "Transaction ids can only be 32 bits";
  COMPILE_ASSERT(sizeof(base::ProcessId) <= sizeof(int32),
                 Process_ID_must_fit_in_32_bits);

  return transaction_id | (static_cast<uint64>(pid) << 32);
}

WebIDBCursor* IndexedDBDispatcherHost::GetCursorFromId(int32 ipc_cursor_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  return cursor_dispatcher_host_->map_.Lookup(ipc_cursor_id);
}

void IndexedDBDispatcherHost::OnIDBFactoryGetDatabaseNames(
    const IndexedDBHostMsg_FactoryGetDatabaseNames_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  FilePath indexed_db_path = indexed_db_context_->data_path();

  WebSecurityOrigin origin(
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin));

  Context()->GetIDBFactory()->getDatabaseNames(
      new IndexedDBCallbacks<WebDOMStringList>(this, params.ipc_thread_id,
      params.ipc_response_id), origin, NULL,
      webkit_base::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::OnIDBFactoryOpenOld(
    const IndexedDBHostMsg_FactoryOpen_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  FilePath indexed_db_path = indexed_db_context_->data_path();

  GURL origin_url = DatabaseUtil::GetOriginFromIdentifier(params.origin);
  WebSecurityOrigin origin(
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin));

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  Context()->GetIDBFactory()->open(
      params.name,
      params.version,
      new IndexedDBCallbacksDatabase(this, params.ipc_thread_id,
                                     params.ipc_response_id, origin_url),
      new IndexedDBDatabaseCallbacks(this, params.ipc_thread_id,
                                     params.ipc_database_response_id),
      origin, NULL, webkit_base::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::OnIDBFactoryOpen(
    const IndexedDBHostMsg_FactoryOpen_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  FilePath indexed_db_path = indexed_db_context_->data_path();

  GURL origin_url = DatabaseUtil::GetOriginFromIdentifier(params.origin);
  WebSecurityOrigin origin(
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin));

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));

  int64 transaction_id = HostTransactionId(params.transaction_id);

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  Context()->GetIDBFactory()->open(
      params.name,
      params.version,
      transaction_id,
      new IndexedDBCallbacksDatabase(this, params.ipc_thread_id,
                                     params.ipc_response_id, origin_url),
      new IndexedDBDatabaseCallbacks(this, params.ipc_thread_id,
                                     params.ipc_database_response_id),
      origin, NULL, webkit_base::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::OnIDBFactoryDeleteDatabase(
    const IndexedDBHostMsg_FactoryDeleteDatabase_Params& params) {
  FilePath indexed_db_path = indexed_db_context_->data_path();

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  Context()->GetIDBFactory()->deleteDatabase(
      params.name,
      new IndexedDBCallbacks<WebSerializedScriptValue>(this,
                                                       params.ipc_thread_id,
                                                       params.ipc_response_id),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin), NULL,
      webkit_base::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::TransactionComplete(int32 ipc_transaction_id) {
  Context()->TransactionComplete(
      transaction_dispatcher_host_->transaction_url_map_[ipc_transaction_id]);
}

//////////////////////////////////////////////////////////////////////
// Helper templates.
//

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 ipc_return_object_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  ObjectType* return_object = map->Lookup(ipc_return_object_id);
  if (!return_object) {
    NOTREACHED() << "Uh oh, couldn't find object with id "
                 << ipc_return_object_id;
    RecordAction(UserMetricsAction("BadMessageTerminate_IDBMF"));
    BadMessageReceived();
  }
  return return_object;
}

template <typename ObjectType>
void IndexedDBDispatcherHost::DestroyObject(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 ipc_object_id) {
  GetOrTerminateProcess(map, ipc_object_id);
  map->Remove(ipc_object_id);
}


//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::DatabaseDispatcherHost
//

IndexedDBDispatcherHost::DatabaseDispatcherHost::DatabaseDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::DatabaseDispatcherHost::~DatabaseDispatcherHost() {
  for (WebIDBObjectIDToURLMap::iterator iter = database_url_map_.begin();
       iter != database_url_map_.end(); iter++) {
    WebIDBDatabase* database = map_.Lookup(iter->first);
    if (database) {
      database->close();
      parent_->Context()->ConnectionClosed(iter->second, database);
    }
  }
}

bool IndexedDBDispatcherHost::DatabaseDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::DatabaseDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseMetadata, OnMetadata)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCreateObjectStore,
                        OnCreateObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDeleteObjectStore,
                        OnDeleteObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCreateTransaction,
                        OnCreateTransaction)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseClose, OnClose)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDestroyed, OnDestroyed)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseGet, OnGet)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabasePut, OnPut)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseSetIndexKeys,
                        OnSetIndexKeys)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseSetIndexesReady,
                        OnSetIndexesReady)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseOpenCursor, OnOpenCursor)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCount, OnCount)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDeleteRange, OnDeleteRange)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseClear, OnClear)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::Send(
    IPC::Message* message) {
  parent_->Send(message);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnMetadata(
    int32 ipc_database_id, IndexedDBDatabaseMetadata* metadata) {
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, ipc_database_id);
  if (!idb_database)
    return;

  WebIDBMetadata web_metadata = idb_database->metadata();
  metadata->id = web_metadata.id;
  metadata->name = web_metadata.name;
  metadata->version = web_metadata.version;
  metadata->int_version = web_metadata.intVersion;
  metadata->max_object_store_id = web_metadata.maxObjectStoreId;

  for (size_t i = 0; i < web_metadata.objectStores.size(); ++i) {
    const WebIDBMetadata::ObjectStore& web_store_metadata =
        web_metadata.objectStores[i];
    IndexedDBObjectStoreMetadata idb_store_metadata;
    idb_store_metadata.id = web_store_metadata.id;
    idb_store_metadata.name = web_store_metadata.name;
    idb_store_metadata.keyPath = IndexedDBKeyPath(web_store_metadata.keyPath);
    idb_store_metadata.autoIncrement = web_store_metadata.autoIncrement;
    idb_store_metadata.max_index_id = web_store_metadata.maxIndexId;

    for (size_t j = 0; j < web_store_metadata.indexes.size(); ++j) {
      const WebIDBMetadata::Index& web_index_metadata =
          web_store_metadata.indexes[j];
      IndexedDBIndexMetadata idb_index_metadata;
      idb_index_metadata.id = web_index_metadata.id;
      idb_index_metadata.name = web_index_metadata.name;
      idb_index_metadata.keyPath = IndexedDBKeyPath(web_index_metadata.keyPath);
      idb_index_metadata.unique = web_index_metadata.unique;
      idb_index_metadata.multiEntry = web_index_metadata.multiEntry;
      idb_store_metadata.indexes.push_back(idb_index_metadata);
    }
    metadata->object_stores.push_back(idb_store_metadata);
  }
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateObjectStore(
    const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params,
    int32* object_store_id, WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_database || !idb_transaction)
    return;

  *ec = 0;
  WebIDBObjectStore* object_store = idb_database->createObjectStore(
      params.id, params.name, params.key_path, params.auto_increment,
      *idb_transaction, *ec);
  *object_store_id = *ec ? 0 : parent_->Add(object_store);
  if (parent_->Context()->IsOverQuota(
      database_url_map_[params.ipc_database_id])) {
    idb_transaction->abort();
  }
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteObjectStore(
    int32 ipc_database_id,
    int64 index_id,
    int32 ipc_transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, ipc_database_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_database || !idb_transaction)
    return;

  *ec = 0;
  idb_database->deleteObjectStore(index_id, *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateTransaction(
    int32 ipc_thread_id,
    int32 ipc_database_id,
    int64 transaction_id,
    const std::vector<int64>& object_store_ids,
    int32 mode,
    int32* ipc_transaction_id) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, ipc_database_id);
  if (!database)
      return;

  WebVector<long long> object_stores(object_store_ids.size());
  for (size_t i = 0; i < object_store_ids.size(); ++i)
      object_stores[i] = object_store_ids[i];

  transaction_id = parent_->HostTransactionId(transaction_id);

  WebIDBTransaction* transaction = database->createTransaction(
      transaction_id, object_stores, mode);
  *ipc_transaction_id = parent_->Add(transaction, ipc_thread_id,
                                     database_url_map_[ipc_database_id]);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnClose(
    int32 ipc_database_id) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, ipc_database_id);
  if (!database)
    return;
  database->close();
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  WebIDBDatabase* database = map_.Lookup(ipc_object_id);
  parent_->Context()->ConnectionClosed(database_url_map_[ipc_object_id],
                                       database);
  database_url_map_.erase(ipc_object_id);
  parent_->DestroyObject(&map_, ipc_object_id);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnGet(
    const IndexedDBHostMsg_DatabaseGet_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  if (!database)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(
          parent_, params.ipc_thread_id,
          params.ipc_response_id));
  database->get(parent_->HostTransactionId(params.transaction_id),
                params.object_store_id,
                params.index_id,
                params.key_range, params.key_only, callbacks.release());
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnPut(
    const IndexedDBHostMsg_DatabasePut_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));

  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  if (!database)
    return;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, params.ipc_thread_id,
                                        params.ipc_response_id));

  WebVector<unsigned char> value(params.value);
  database->put(parent_->HostTransactionId(params.transaction_id),
                params.object_store_id,
                &value, params.key,
                params.put_mode, callbacks.release(),
                params.index_ids,
                params.index_keys);
  WebIDBTransactionIDToSizeMap* map =
      &parent_->database_dispatcher_host_->transaction_size_map_;
  // Size can't be big enough to overflow because it represents the
  // actual bytes passed through IPC.
  int64 transaction_id = parent_->HostTransactionId(params.transaction_id);
  (*map)[transaction_id] += params.value.size();
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetIndexKeys(
    const IndexedDBHostMsg_DatabaseSetIndexKeys_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  if (!database)
    return;

  database->setIndexKeys(parent_->HostTransactionId(params.transaction_id),
                         params.object_store_id,
                         params.primary_key, params.index_ids,
                         params.index_keys);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetIndexesReady(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    const std::vector<int64>& index_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, ipc_database_id);
  if (!database)
    return;

  database->setIndexesReady(parent_->HostTransactionId(transaction_id),
                            object_store_id,
                            WebVector<long long>(index_ids));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnOpenCursor(
    const IndexedDBHostMsg_DatabaseOpenCursor_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  if (!database)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.ipc_thread_id,
                                           params.ipc_response_id, -1));
  database->openCursor(
      parent_->HostTransactionId(params.transaction_id),
      params.object_store_id, params.index_id,
      params.key_range, params.direction, params.key_only, params.task_type,
      callbacks.release());
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCount(
    const IndexedDBHostMsg_DatabaseCount_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  if (!database)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(
          parent_, params.ipc_thread_id,
          params.ipc_response_id));
  database->count(
      parent_->HostTransactionId(params.transaction_id),
      params.object_store_id, params.index_id,
      params.key_range, callbacks.release());
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteRange(
    const IndexedDBHostMsg_DatabaseDeleteRange_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, params.ipc_database_id);
  if (!database)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(
          parent_, params.ipc_thread_id,
          params.ipc_response_id));
  database->deleteRange(parent_->HostTransactionId(params.transaction_id),
                        params.object_store_id,
                        params.key_range, callbacks.release());
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnClear(
    int32 ipc_thread_id,
    int32 ipc_response_id,
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, ipc_database_id);
  if (!database)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(
          parent_, ipc_thread_id,
          ipc_response_id));

  database->clear(parent_->HostTransactionId(transaction_id),
                  object_store_id, callbacks.release());
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::IndexDispatcherHost
//

IndexedDBDispatcherHost::IndexDispatcherHost::IndexDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::IndexDispatcherHost::~IndexDispatcherHost() {
}

bool IndexedDBDispatcherHost::IndexDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::IndexDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexOpenObjectCursor,
                                    OnOpenObjectCursor)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexOpenKeyCursor, OnOpenKeyCursor)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexCount, OnCount)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexGetObject, OnGetObject)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexGetKey, OnGetKey)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::IndexDispatcherHost::Send(
    IPC::Message* message) {
  parent_->Send(message);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenObjectCursor(
    const IndexedDBHostMsg_IndexOpenCursor_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.ipc_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.ipc_thread_id,
                                           params.ipc_response_id, -1));
  idb_index->openObjectCursor(
      params.key_range, params.direction, callbacks.release(),
      *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenKeyCursor(
    const IndexedDBHostMsg_IndexOpenCursor_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.ipc_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.ipc_thread_id,
                                           params.ipc_response_id, -1));
  idb_index->openKeyCursor(
      params.key_range, params.direction, callbacks.release(),
      *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnCount(
    const IndexedDBHostMsg_IndexCount_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.ipc_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_,
                                                       params.ipc_thread_id,
                                                       params.ipc_response_id));
  idb_index->count(
      params.key_range, callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetObject(
    int ipc_index_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const IndexedDBKeyRange& key_range,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, ipc_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, ipc_thread_id,
                                                       ipc_response_id));
  idb_index->getObject(key_range, callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetKey(
    int ipc_index_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const IndexedDBKeyRange& key_range,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, ipc_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, ipc_thread_id,
                                        ipc_response_id));
  idb_index->getKey(key_range, callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  parent_->DestroyObject(&map_, ipc_object_id);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::ObjectStoreDispatcherHost
//

IndexedDBDispatcherHost::ObjectStoreDispatcherHost::ObjectStoreDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::
ObjectStoreDispatcherHost::~ObjectStoreDispatcherHost() {
}

bool IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::ObjectStoreDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreGet, OnGet)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStorePut, OnPut)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreSetIndexKeys,
                        OnSetIndexKeys)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreSetIndexesReady,
                        OnSetIndexesReady)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDelete, OnDelete)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreClear, OnClear)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreCreateIndex, OnCreateIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreIndex, OnIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDeleteIndex, OnDeleteIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreOpenCursor, OnOpenCursor)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreCount, OnCount)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::Send(
    IPC::Message* message) {
  parent_->Send(message);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnGet(
    int ipc_object_store_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const IndexedDBKeyRange& key_range,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, ipc_thread_id,
                                                       ipc_response_id));
  idb_object_store->get(key_range, callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnPut(
    const IndexedDBHostMsg_ObjectStorePut_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, params.ipc_thread_id,
                                        params.ipc_response_id));
  idb_object_store->put(params.serialized_value, params.key,
                        params.put_mode, callbacks.release(),
                        *idb_transaction, params.index_ids,
                        params.index_keys);
  int64 size = UTF16ToUTF8(params.serialized_value.data()).size();
  WebIDBTransactionIPCIDToSizeMap* map =
      &parent_->transaction_dispatcher_host_->transaction_ipc_size_map_;
  (*map)[params.ipc_transaction_id] += size;
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnSetIndexKeys(
    int32 ipc_object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<int64>& index_names,
    const std::vector<std::vector<IndexedDBKey> >& index_keys,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;
  idb_object_store->setIndexKeys(primary_key,
                                 WebVector<long long>(index_names),
                                 index_keys, *idb_transaction);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnSetIndexesReady(
    int32 ipc_object_store_id,
    const std::vector<int64>& index_ids,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  idb_object_store->setIndexesReady(WebVector<long long>(index_ids),
                                    *idb_transaction);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDelete(
    int ipc_object_store_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const IndexedDBKeyRange& key_range,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, ipc_thread_id,
                                                       ipc_response_id));
  idb_object_store->deleteFunction(
      key_range, callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnClear(
    int ipc_object_store_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    int32 ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, ipc_thread_id,
                                                       ipc_response_id));
  idb_object_store->clear(callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCreateIndex(
    const IndexedDBHostMsg_ObjectStoreCreateIndex_Params& params,
    int32* index_id, WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_object_store || !idb_transaction)
    return;

  *ec = 0;
  WebIDBIndex* index = idb_object_store->createIndex(
      params.id, params.name, params.key_path, params.unique,
      params.multi_entry, *idb_transaction, *ec);
  *index_id = *ec ? 0 : parent_->Add(index);
  WebIDBObjectIDToURLMap* transaction_url_map =
      &parent_->transaction_dispatcher_host_->transaction_url_map_;
  if (parent_->Context()->IsOverQuota(
      (*transaction_url_map)[params.ipc_transaction_id])) {
    idb_transaction->abort();
  }
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndex(
    int32 ipc_object_store_id,
    int64 index_id,
    int32* ipc_index_id) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  if (!idb_object_store)
    return;

  WebIDBIndex* index = idb_object_store->index(index_id);
  *ipc_index_id = parent_->Add(index);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDeleteIndex(
    int32 ipc_object_store_id,
    int64 index_id,
    int32 ipc_transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, ipc_transaction_id);
  if (!idb_object_store || !idb_transaction)
    return;

  *ec = 0;
  idb_object_store->deleteIndex(index_id, *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnOpenCursor(
    const IndexedDBHostMsg_ObjectStoreOpenCursor_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.ipc_thread_id,
                                           params.ipc_response_id, -1));
  idb_object_store->openCursor(
      params.key_range, params.direction, callbacks.release(),
      params.task_type,
      *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCount(
    const IndexedDBHostMsg_ObjectStoreCount_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.ipc_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.ipc_transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  WebKit::WebExceptionCode ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_,
                                                       params.ipc_thread_id,
                                                       params.ipc_response_id));
  idb_object_store->count(
      params.key_range, callbacks.release(), *idb_transaction, ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  parent_->DestroyObject(&map_, ipc_object_id);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::CursorDispatcherHost
//

IndexedDBDispatcherHost::CursorDispatcherHost::CursorDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::CursorDispatcherHost::~CursorDispatcherHost() {
}

bool IndexedDBDispatcherHost::CursorDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::CursorDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorAdvance, OnAdvance)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorContinue, OnContinue)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorPrefetch, OnPrefetch)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorPrefetchReset, OnPrefetchReset)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorDelete, OnDelete)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}


void IndexedDBDispatcherHost::CursorDispatcherHost::Send(
    IPC::Message* message) {
  parent_->Send(message);
}


void IndexedDBDispatcherHost::CursorDispatcherHost::OnAdvance(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    unsigned long count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, ipc_cursor_id);
  if (!idb_cursor)
    return;

  WebKit::WebExceptionCode ec = 0;
  idb_cursor->advance(count,
                      new IndexedDBCallbacks<WebIDBCursor>(parent_,
                                                           ipc_thread_id,
                                                           ipc_response_id,
                                                           ipc_cursor_id),
                      ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnContinue(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const IndexedDBKey& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_,
                                                            ipc_cursor_id);
  if (!idb_cursor)
    return;

  WebKit::WebExceptionCode ec = 0;
  idb_cursor->continueFunction(
      key, new IndexedDBCallbacks<WebIDBCursor>(parent_, ipc_thread_id,
                                                ipc_response_id,
                                                ipc_cursor_id), ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrefetch(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    int n) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_,
                                                            ipc_cursor_id);
  if (!idb_cursor)
    return;

  WebKit::WebExceptionCode ec = 0;
  idb_cursor->prefetchContinue(
      n, new IndexedDBCallbacks<WebIDBCursor>(parent_, ipc_thread_id,
                                              ipc_response_id,
                                              ipc_cursor_id), ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrefetchReset(
    int32 ipc_cursor_id, int used_prefetches, int unused_prefetches) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_,
                                                            ipc_cursor_id);
  if (!idb_cursor)
    return;

  idb_cursor->prefetchReset(used_prefetches, unused_prefetches);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDelete(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_response_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_,
                                                            ipc_cursor_id);
  if (!idb_cursor)
    return;

  WebKit::WebExceptionCode ec = 0;
  idb_cursor->deleteFunction(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, ipc_thread_id,
                                                       ipc_response_id), ec);
  DCHECK(!ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  parent_->DestroyObject(&map_, ipc_object_id);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::TransactionDispatcherHost
//

IndexedDBDispatcherHost::TransactionDispatcherHost::TransactionDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::
    TransactionDispatcherHost::~TransactionDispatcherHost() {
  MapType::iterator it(&map_);
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->abort();
    it.Advance();
  }
}

bool IndexedDBDispatcherHost::TransactionDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::TransactionDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionCommit, OnCommit)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionObjectStore, OnObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionDidCompleteTaskEvents,
                        OnDidCompleteTaskEvents)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::Send(
    IPC::Message* message) {
  parent_->Send(message);
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnCommit(
    int32 ipc_transaction_id) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, ipc_transaction_id);
  if (!idb_transaction)
    return;

  // TODO(dgrogan): Tell the page the transaction aborted because of quota.
  // http://crbug.com/113118
  if (parent_->Context()->WouldBeOverQuota(
      transaction_url_map_[ipc_transaction_id],
      transaction_ipc_size_map_[ipc_transaction_id])) {
    idb_transaction->abort();
    return;
  }

  idb_transaction->commit();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnAbort(
    int32 ipc_transaction_id) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, ipc_transaction_id);
  if (!idb_transaction)
    return;

  idb_transaction->abort();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnObjectStore(
    int32 ipc_transaction_id, int64 object_store_id, int32* ipc_object_store_id,
    WebKit::WebExceptionCode* ec) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, ipc_transaction_id);
  if (!idb_transaction)
    return;

  *ec = 0;
  WebIDBObjectStore* object_store =
          idb_transaction->objectStore(object_store_id, *ec);
  *ipc_object_store_id = object_store ? parent_->Add(object_store) : 0;
}

void IndexedDBDispatcherHost::
    TransactionDispatcherHost::OnDidCompleteTaskEvents(int ipc_transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, ipc_transaction_id);
  if (!idb_transaction)
    return;
  idb_transaction->didCompleteTaskEvents();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  // TODO(dgrogan): This doesn't seem to be happening with some version change
  // transactions. Possibly introduced with integer version support.
  transaction_ipc_size_map_.erase(ipc_object_id);
  transaction_url_map_.erase(ipc_object_id);
  parent_->DestroyObject(&map_, ipc_object_id);
}

}  // namespace content
