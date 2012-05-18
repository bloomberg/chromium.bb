// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"

#include "base/bind.h"
#include "base/command_line.h"
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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBIndex.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "webkit/database/database_util.h"
#include "webkit/glue/webkit_glue.h"

using content::BrowserMessageFilter;
using content::BrowserThread;
using content::IndexedDBKey;
using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;
using content::UserMetricsAction;
using content::SerializedScriptValue;
using webkit_database::DatabaseUtil;
using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBCursor;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBIndex;
using WebKit::WebIDBKey;
using WebKit::WebIDBKeyPath;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransaction;
using WebKit::WebSecurityOrigin;
using WebKit::WebSerializedScriptValue;
using WebKit::WebVector;

namespace {

template <class T>
void DeleteOnWebKitThread(T* obj) {
  if (!BrowserThread::DeleteSoon(BrowserThread::WEBKIT_DEPRECATED,
                                 FROM_HERE, obj))
    delete obj;
}

}

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int process_id, IndexedDBContextImpl* indexed_db_context)
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
      process_id_(process_id) {
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
                                   int32 thread_id,
                                   const GURL& origin_url) {
  if (!database_dispatcher_host_.get()) {
    delete idb_database;
    return 0;
  }
  int32 idb_database_id = database_dispatcher_host_->map_.Add(idb_database);
  Context()->ConnectionOpened(origin_url);
  database_dispatcher_host_->database_url_map_[idb_database_id] = origin_url;
  return idb_database_id;
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
                                   int32 thread_id,
                                   const GURL& url) {
  if (!transaction_dispatcher_host_.get()) {
    delete idb_transaction;
    return 0;
  }
  int32 id = transaction_dispatcher_host_->map_.Add(idb_transaction);
  idb_transaction->setCallbacks(
      new IndexedDBTransactionCallbacks(this, thread_id, id));
  transaction_dispatcher_host_->transaction_url_map_[id] = url;
  return id;
}

WebIDBCursor* IndexedDBDispatcherHost::GetCursorFromId(int32 cursor_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  return cursor_dispatcher_host_->map_.Lookup(cursor_id);
}

void IndexedDBDispatcherHost::OnIDBFactoryGetDatabaseNames(
    const IndexedDBHostMsg_FactoryGetDatabaseNames_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  FilePath indexed_db_path = indexed_db_context_->data_path();

  WebSecurityOrigin origin(
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin));

  Context()->GetIDBFactory()->getDatabaseNames(
      new IndexedDBCallbacks<WebDOMStringList>(this, params.thread_id,
      params.response_id), origin, NULL,
      webkit_glue::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::OnIDBFactoryOpen(
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
      new IndexedDBCallbacks<WebIDBDatabase>(this, params.thread_id,
                                             params.response_id, origin_url),
      origin, NULL, webkit_glue::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::OnIDBFactoryDeleteDatabase(
    const IndexedDBHostMsg_FactoryDeleteDatabase_Params& params) {
  FilePath indexed_db_path = indexed_db_context_->data_path();

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  Context()->GetIDBFactory()->deleteDatabase(
      params.name,
      new IndexedDBCallbacks<WebSerializedScriptValue>(this,
                                                       params.thread_id,
                                                       params.response_id),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin), NULL,
      webkit_glue::FilePathToWebString(indexed_db_path));
}

void IndexedDBDispatcherHost::TransactionComplete(int32 transaction_id) {
  Context()->TransactionComplete(
      transaction_dispatcher_host_->transaction_url_map_[transaction_id]);
}

//////////////////////////////////////////////////////////////////////
// Helper templates.
//

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 return_object_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  ObjectType* return_object = map->Lookup(return_object_id);
  if (!return_object) {
    content::RecordAction(UserMetricsAction("BadMessageTerminate_IDBMF"));
    BadMessageReceived();
  }
  return return_object;
}

template <typename ReplyType, typename MapObjectType, typename Method>
void IndexedDBDispatcherHost::SyncGetter(
    IDMap<MapObjectType, IDMapOwnPointer>* map, int32 object_id,
    ReplyType* reply, Method method) {
  MapObjectType* object = GetOrTerminateProcess(map, object_id);
  if (!object)
      return;

  *reply = (object->*method)();
}

template <typename ObjectType>
void IndexedDBDispatcherHost::DestroyObject(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 object_id) {
  GetOrTerminateProcess(map, object_id);
  map->Remove(object_id);
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
    if (database)
      database->close();
    parent_->Context()->ConnectionClosed(iter->second);
  }
}

bool IndexedDBDispatcherHost::DatabaseDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::DatabaseDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseName, OnName)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseVersion, OnVersion)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseObjectStoreNames,
                        OnObjectStoreNames)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCreateObjectStore,
                        OnCreateObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDeleteObjectStore,
                        OnDeleteObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseSetVersion, OnSetVersion)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseTransaction, OnTransaction)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseClose, OnClose)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseOpen, OnOpen)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::Send(
    IPC::Message* message) {
  parent_->Send(message);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnName(
    int32 object_id, string16* name) {
  parent_->SyncGetter<string16>(&map_, object_id, name, &WebIDBDatabase::name);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnVersion(
    int32 object_id, string16* version) {
  parent_->SyncGetter<string16>(
      &map_, object_id, version, &WebIDBDatabase::version);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnObjectStoreNames(
    int32 idb_database_id, std::vector<string16>* object_stores) {
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  if (!idb_database)
    return;

  WebDOMStringList web_object_stores = idb_database->objectStoreNames();
  object_stores->reserve(web_object_stores.length());
  for (unsigned i = 0; i < web_object_stores.length(); ++i)
    object_stores->push_back(web_object_stores.item(i));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateObjectStore(
    const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params,
    int32* object_store_id, WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, params.idb_database_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_database || !idb_transaction)
    return;

  *ec = 0;
  WebIDBObjectStore* object_store = idb_database->createObjectStore(
      params.name, params.key_path, params.auto_increment,
      *idb_transaction, *ec);
  *object_store_id = *ec ? 0 : parent_->Add(object_store);
  if (parent_->Context()->IsOverQuota(
      database_url_map_[params.idb_database_id])) {
    idb_transaction->abort();
  }
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteObjectStore(
    int32 idb_database_id,
    const string16& name,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_database || !idb_transaction)
    return;

  *ec = 0;
  idb_database->deleteObjectStore(name, *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetVersion(
    int32 idb_database_id,
    int32 thread_id,
    int32 response_id,
    const string16& version,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  if (!idb_database)
    return;

  *ec = 0;
  idb_database->setVersion(
      version,
      new IndexedDBCallbacks<WebIDBTransaction>(parent_, thread_id, response_id,
          database_url_map_[idb_database_id]),
      *ec);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnTransaction(
    int32 thread_id,
    int32 idb_database_id,
    const std::vector<string16>& names,
    int32 mode,
    int32* idb_transaction_id,
    WebKit::WebExceptionCode* ec) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  if (!database)
      return;

  WebDOMStringList object_stores;
  for (std::vector<string16>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    object_stores.append(*it);
  }

  *ec = 0;
  WebIDBTransaction* transaction = database->transaction(
      object_stores, mode, *ec);
  DCHECK(!transaction != !*ec);
  *idb_transaction_id =
      *ec ? 0 : parent_->Add(transaction, thread_id,
                             database_url_map_[idb_database_id]);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnOpen(
    int32 idb_database_id, int32 thread_id, int32 response_id) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  if (!database)
    return;
  database->open(new IndexedDBDatabaseCallbacks(parent_, thread_id,
                                                response_id));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnClose(
    int32 idb_database_id) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  if (!database)
    return;
  database->close();
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->Context()->ConnectionClosed(database_url_map_[object_id]);
  database_url_map_.erase(object_id);
  parent_->DestroyObject(&map_, object_id);
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexName, OnName)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexKeyPath, OnKeyPath)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexUnique, OnUnique)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexMultiEntry, OnMultiEntry)
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

void IndexedDBDispatcherHost::IndexDispatcherHost::OnName(
    int32 object_id, string16* name) {
  parent_->SyncGetter<string16>(&map_, object_id, name, &WebIDBIndex::name);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnKeyPath(
    int32 object_id, IndexedDBKeyPath* key_path) {
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(&map_, object_id);
  if (!idb_index)
    return;

  *key_path = IndexedDBKeyPath(idb_index->keyPath());
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnUnique(
    int32 object_id, bool* unique) {
  parent_->SyncGetter<bool>(&map_, object_id, unique, &WebIDBIndex::unique);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnMultiEntry(
    int32 object_id, bool* multi_entry) {
  parent_->SyncGetter<bool>(
      &map_, object_id, multi_entry, &WebIDBIndex::multiEntry);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenObjectCursor(
    const IndexedDBHostMsg_IndexOpenCursor_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.thread_id,
                                           params.response_id, -1));
  idb_index->openObjectCursor(
      params.key_range, params.direction, callbacks.release(),
      *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenKeyCursor(
    const IndexedDBHostMsg_IndexOpenCursor_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.thread_id,
                                           params.response_id, -1));
  idb_index->openKeyCursor(
      params.key_range, params.direction, callbacks.release(),
      *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnCount(
    const IndexedDBHostMsg_IndexCount_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_,
                                                       params.thread_id,
                                                       params.response_id));
  idb_index->count(
      params.key_range, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetObject(
    int idb_index_id,
    int32 thread_id,
    int32 response_id,
    const IndexedDBKeyRange& key_range,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, thread_id,
                                                       response_id));
  idb_index->getObject(key_range, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetKey(
    int idb_index_id,
    int32 thread_id,
    int32 response_id,
    const IndexedDBKeyRange& key_range,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, thread_id, response_id));
  idb_index->getKey(key_range, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id);
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreName, OnName)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreKeyPath, OnKeyPath)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreIndexNames, OnIndexNames)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreAutoIncrement,
                        OnAutoIncrement)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreGet, OnGet)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStorePut, OnPut)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDelete, OnDelete)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDeleteRange, OnDeleteRange)
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

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnName(
    int32 object_id, string16* name) {
  parent_->SyncGetter<string16>(
      &map_, object_id, name, &WebIDBObjectStore::name);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnKeyPath(
    int32 object_id, IndexedDBKeyPath* key_path) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_,object_id);
  if (!idb_object_store)
    return;

  *key_path = IndexedDBKeyPath(idb_object_store->keyPath());
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndexNames(
    int32 idb_object_store_id, std::vector<string16>* index_names) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  if (!idb_object_store)
    return;

  WebDOMStringList web_index_names = idb_object_store->indexNames();
  index_names->reserve(web_index_names.length());
  for (unsigned i = 0; i < web_index_names.length(); ++i)
    index_names->push_back(web_index_names.item(i));
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnAutoIncrement(
    int32 idb_object_store_id, bool* auto_increment) {
  parent_->SyncGetter<bool>(&map_, idb_object_store_id, auto_increment,
                            &WebIDBObjectStore::autoIncrement);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnGet(
    int idb_object_store_id,
    int32 thread_id,
    int32 response_id,
    const IndexedDBKeyRange& key_range,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, thread_id,
                                                       response_id));
  idb_object_store->get(key_range, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnPut(
    const IndexedDBHostMsg_ObjectStorePut_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, params.thread_id,
                                        params.response_id));
  idb_object_store->put(params.serialized_value, params.key, params.put_mode,
                        callbacks.release(), *idb_transaction, *ec);
  if (*ec)
    return;
  int64 size = UTF16ToUTF8(params.serialized_value.data()).size();
  WebIDBTransactionIDToSizeMap* map =
      &parent_->transaction_dispatcher_host_->transaction_size_map_;
  (*map)[params.transaction_id] += size;
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDelete(
    int idb_object_store_id,
    int32 thread_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, thread_id,
                                                       response_id));
  idb_object_store->deleteFunction(
      key, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDeleteRange(
    int idb_object_store_id,
    int32 thread_id,
    int32 response_id,
    const IndexedDBKeyRange& key_range,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, thread_id,
                                                       response_id));
  idb_object_store->deleteFunction(
      key_range, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnClear(
    int idb_object_store_id,
    int32 thread_id,
    int32 response_id,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, thread_id,
                                                       response_id));
  idb_object_store->clear(callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCreateIndex(
   const IndexedDBHostMsg_ObjectStoreCreateIndex_Params& params,
   int32* index_id, WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_object_store || !idb_transaction)
    return;

  *ec = 0;
  WebIDBIndex* index = idb_object_store->createIndex(
      params.name, params.key_path, params.unique, params.multi_entry,
      *idb_transaction, *ec);
  *index_id = *ec ? 0 : parent_->Add(index);
  WebIDBObjectIDToURLMap* transaction_url_map =
      &parent_->transaction_dispatcher_host_->transaction_url_map_;
  if (parent_->Context()->IsOverQuota(
      (*transaction_url_map)[params.transaction_id])) {
    idb_transaction->abort();
  }
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndex(
    int32 idb_object_store_id,
    const string16& name,
    int32* idb_index_id,
    WebKit::WebExceptionCode* ec) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  if (!idb_object_store)
    return;

  *ec = 0;
  WebIDBIndex* index = idb_object_store->index(name, *ec);
  *idb_index_id = parent_->Add(index);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDeleteIndex(
    int32 idb_object_store_id,
    const string16& name,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_object_store || !idb_transaction)
    return;

  *ec = 0;
  idb_object_store->deleteIndex(name, *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnOpenCursor(
    const IndexedDBHostMsg_ObjectStoreOpenCursor_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.thread_id,
                                           params.response_id, -1));
  idb_object_store->openCursor(
      params.key_range, params.direction, callbacks.release(),
      *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCount(
    const IndexedDBHostMsg_ObjectStoreCount_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_,
                                                       params.thread_id,
                                                       params.response_id));
  idb_object_store->count(
      params.key_range, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id);
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorDirection, OnDirection)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorUpdate, OnUpdate)
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

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDirection(
    int32 object_id, int32* direction) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, object_id);
  if (!idb_cursor)
    return;

  *direction = idb_cursor->direction();
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnKey(
    int32 object_id, IndexedDBKey* key) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, object_id);
  if (!idb_cursor)
    return;

  *key = IndexedDBKey(idb_cursor->key());
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrimaryKey(
    int32 object_id, IndexedDBKey* primary_key) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, object_id);
  if (!idb_cursor)
    return;

  *primary_key = IndexedDBKey(idb_cursor->primaryKey());
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnValue(
    int32 object_id,
    SerializedScriptValue* script_value) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, object_id);
  if (!idb_cursor)
    return;

  *script_value = SerializedScriptValue(idb_cursor->value());
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnUpdate(
    int32 cursor_id,
    int32 thread_id,
    int32 response_id,
    const SerializedScriptValue& value,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->update(
      value, new IndexedDBCallbacks<WebIDBKey>(parent_, thread_id, response_id),
      *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnAdvance(
    int32 cursor_id,
    int32 thread_id,
    int32 response_id,
    unsigned long count,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->advance(count,
                      new IndexedDBCallbacks<WebIDBCursor>(parent_,
                                                           thread_id,
                                                           response_id,
                                                           cursor_id),
                      *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnContinue(
    int32 cursor_id,
    int32 thread_id,
    int32 response_id,
    const IndexedDBKey& key,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->continueFunction(
      key, new IndexedDBCallbacks<WebIDBCursor>(parent_, thread_id, response_id,
                                                cursor_id), *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrefetch(
    int32 cursor_id,
    int32 thread_id,
    int32 response_id,
    int n,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->prefetchContinue(
      n, new IndexedDBCallbacks<WebIDBCursor>(parent_, thread_id, response_id,
                                              cursor_id), *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrefetchReset(
    int32 cursor_id, int used_prefetches, int unused_prefetches) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  idb_cursor->prefetchReset(used_prefetches, unused_prefetches);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDelete(
    int32 cursor_id,
    int32 thread_id,
    int32 response_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->deleteFunction(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, thread_id,
                                                       response_id), *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id);
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_TransactionMode, OnMode)
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

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnAbort(
    int32 transaction_id) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id);
  if (!idb_transaction)
    return;

  idb_transaction->abort();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnMode(
    int32 transaction_id,
    int* mode) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id);
  if (!idb_transaction)
    return;

  *mode = idb_transaction->mode();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnObjectStore(
    int32 transaction_id, const string16& name, int32* object_store_id,
    WebKit::WebExceptionCode* ec) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id);
  if (!idb_transaction)
    return;

  *ec = 0;
  WebIDBObjectStore* object_store = idb_transaction->objectStore(name, *ec);
  *object_store_id = object_store ? parent_->Add(object_store) : 0;
}

void IndexedDBDispatcherHost::
    TransactionDispatcherHost::OnDidCompleteTaskEvents(int transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id);
  if (!idb_transaction)
    return;

  // TODO(dgrogan): Tell the page the transaction aborted because of quota.
  // http://crbug.com/113118
  if (parent_->Context()->WouldBeOverQuota(
      transaction_url_map_[transaction_id],
      transaction_size_map_[transaction_id])) {
    idb_transaction->abort();
    return;
  }
  idb_transaction->didCompleteTaskEvents();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnDestroyed(
    int32 object_id) {
  transaction_size_map_.erase(object_id);
  transaction_url_map_.erase(object_id);
  parent_->DestroyObject(&map_, object_id);
}
