// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/in_process_webkit/indexed_db_callbacks.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/render_message_filter.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/indexed_db_messages.h"
#include "chrome/common/result_codes.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBIndex.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBCursor;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBIndex;
using WebKit::WebIDBKey;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransaction;
using WebKit::WebSecurityOrigin;
using WebKit::WebSerializedScriptValue;
using WebKit::WebVector;

namespace {

// FIXME: Replace this magic constant once we have a more sophisticated quota
// system.
static const uint64 kDefaultQuota = 5 * 1024 * 1024;

template <class T>
void DeleteOnWebKitThread(T* obj) {
  if (!BrowserThread::DeleteSoon(BrowserThread::WEBKIT, FROM_HERE, obj))
    delete obj;
}

}

IndexedDBDispatcherHost::IndexedDBDispatcherHost(int process_id,
                                                 Profile* profile)
    : webkit_context_(profile->GetWebKitContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()),
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
  DCHECK(webkit_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
}

void IndexedDBDispatcherHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();
  BrowserThread::DeleteSoon(
        BrowserThread::WEBKIT, FROM_HERE, database_dispatcher_host_.release());
  BrowserThread::DeleteSoon(
        BrowserThread::WEBKIT, FROM_HERE, index_dispatcher_host_.release());
  BrowserThread::DeleteSoon(
        BrowserThread::WEBKIT, FROM_HERE,
        object_store_dispatcher_host_.release());
  BrowserThread::DeleteSoon(
        BrowserThread::WEBKIT, FROM_HERE, cursor_dispatcher_host_.release());
  BrowserThread::DeleteSoon(
        BrowserThread::WEBKIT, FROM_HERE,
        transaction_dispatcher_host_.release());
}

void IndexedDBDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == IndexedDBMsgStart)
    *thread = BrowserThread::WEBKIT;
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  if (IPC_MESSAGE_CLASS(message) != IndexedDBMsgStart)
    return false;

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));

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

int32 IndexedDBDispatcherHost::Add(WebIDBDatabase* idb_database) {
  if (!database_dispatcher_host_.get()) {
    delete idb_database;
    return 0;
  }
  return database_dispatcher_host_->map_.Add(idb_database);
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

int32 IndexedDBDispatcherHost::Add(WebIDBTransaction* idb_transaction) {
  if (!transaction_dispatcher_host_.get()) {
    delete idb_transaction;
    return 0;
  }
  int32 id = transaction_dispatcher_host_->map_.Add(idb_transaction);
  idb_transaction->setCallbacks(new IndexedDBTransactionCallbacks(this, id));
  return id;
}

bool IndexedDBDispatcherHost::CheckContentSetting(const string16& origin,
                                                  const string16& description,
                                                  int routing_id,
                                                  int response_id) {
  GURL host(string16(WebSecurityOrigin::createFromDatabaseIdentifier(
      origin).toString()));

  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          host, CONTENT_SETTINGS_TYPE_COOKIES, "");

  CallRenderViewHostContentSettingsDelegate(
      process_id_, routing_id,
      &RenderViewHostDelegate::ContentSettings::OnIndexedDBAccessed,
      host, description, content_setting == CONTENT_SETTING_BLOCK);

  if (content_setting == CONTENT_SETTING_BLOCK) {
    // TODO(jorlow): Change this to the proper error code once we figure out
    // one.
    int error_code = 0; // Defined by the IndexedDB spec.
    static string16 error_message = ASCIIToUTF16(
        "The user denied permission to access the database.");
    Send(new IndexedDBMsg_CallbacksError(response_id, error_code,
                                         error_message));
    return false;
  }
  return true;
}

void IndexedDBDispatcherHost::OnIDBFactoryOpen(
    const IndexedDBHostMsg_FactoryOpen_Params& params) {
  FilePath base_path = webkit_context_->data_path();
  FilePath indexed_db_path;
  if (!base_path.empty()) {
    indexed_db_path = base_path.Append(
        IndexedDBContext::kIndexedDBDirectory);
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  if (!CheckContentSetting(params.origin, params.name, params.routing_id,
                           params.response_id)) {
    return;
  }

  DCHECK(kDefaultQuota == params.maximum_size);

  uint64 quota = kDefaultQuota;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUnlimitedQuotaForIndexedDB)) {
      quota = 1024 * 1024 * 1024; // 1GB. More or less "unlimited".
  }

  Context()->GetIDBFactory()->open(
      params.name,
      new IndexedDBCallbacks<WebIDBDatabase>(this, params.response_id),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin), NULL,
      webkit_glue::FilePathToWebString(indexed_db_path), quota);
}

void IndexedDBDispatcherHost::OnIDBFactoryDeleteDatabase(
    const IndexedDBHostMsg_FactoryDeleteDatabase_Params& params) {
  FilePath base_path = webkit_context_->data_path();
  FilePath indexed_db_path;
  if (!base_path.empty()) {
    indexed_db_path = base_path.Append(
        IndexedDBContext::kIndexedDBDirectory);
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  if (!CheckContentSetting(params.origin, params.name, params.routing_id,
                           params.response_id)) {
    return;
  }

  Context()->GetIDBFactory()->deleteDatabase(
      params.name,
      new IndexedDBCallbacks<WebIDBDatabase>(this, params.response_id),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin), NULL,
      webkit_glue::FilePathToWebString(indexed_db_path));
}

//////////////////////////////////////////////////////////////////////
// Helper templates.
//

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 return_object_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  ObjectType* return_object = map->Lookup(return_object_id);
  if (!return_object) {
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_IDBMF"));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
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
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteObjectStore(
    int32 idb_database_id,
    const string16& name,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
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
    int32 response_id,
    const string16& version,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  if (!idb_database)
    return;

  *ec = 0;
  idb_database->setVersion(
      version,
      new IndexedDBCallbacks<WebIDBTransaction>(parent_, response_id),
      *ec);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnTransaction(
    int32 idb_database_id,
    const std::vector<string16>& names,
    int32 mode,
    int32 timeout,
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
      object_stores, mode, timeout, *ec);
  DCHECK(!transaction != !*ec);
  *idb_transaction_id = *ec ? 0 : parent_->Add(transaction);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnClose(
    int32 idb_database_id) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id);
  database->close();
}

 void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDestroyed(
    int32 object_id) {
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexStoreName, OnStoreName)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexKeyPath, OnKeyPath)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexUnique, OnUnique)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexOpenObjectCursor,
                                    OnOpenObjectCursor)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_IndexOpenKeyCursor, OnOpenKeyCursor)
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

void IndexedDBDispatcherHost::IndexDispatcherHost::OnStoreName(
    int32 object_id, string16* store_name) {
  parent_->SyncGetter<string16>(
      &map_, object_id, store_name, &WebIDBIndex::storeName);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnKeyPath(
    int32 object_id, NullableString16* key_path) {
  parent_->SyncGetter<NullableString16>(
      &map_, object_id, key_path, &WebIDBIndex::keyPath);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnUnique(
    int32 object_id, bool* unique) {
  parent_->SyncGetter<bool>(&map_, object_id, unique, &WebIDBIndex::unique);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenObjectCursor(
    const IndexedDBHostMsg_IndexOpenCursor_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id));
  idb_index->openObjectCursor(
      WebIDBKeyRange(params.lower_key, params.upper_key, params.lower_open,
                     params.upper_open),
      params.direction, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenKeyCursor(
    const IndexedDBHostMsg_IndexOpenCursor_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id));
  idb_index->openKeyCursor(
      WebIDBKeyRange(params.lower_key, params.upper_key, params.lower_open,
                     params.upper_open),
      params.direction, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetObject(
    int idb_index_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id));
  idb_index->getObject(key, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetKey(
    int idb_index_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, idb_index_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_index)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, response_id));
  idb_index->getKey(key, callbacks.release(), *idb_transaction, *ec);
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreGet, OnGet)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStorePut, OnPut)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDelete, OnDelete)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreClear, OnClear)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreCreateIndex, OnCreateIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreIndex, OnIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreDeleteIndex, OnDeleteIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_ObjectStoreOpenCursor, OnOpenCursor)
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
    int32 object_id, NullableString16* keyPath) {
  parent_->SyncGetter<NullableString16>(
      &map_, object_id, keyPath, &WebIDBObjectStore::keyPath);
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

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnGet(
    int idb_object_store_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id));
  idb_object_store->get(key, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnPut(
    const IndexedDBHostMsg_ObjectStorePut_Params& params,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, params.response_id));
  idb_object_store->put(params.serialized_value, params.key, params.put_mode,
                        callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDelete(
    int idb_object_store_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id));
  idb_object_store->deleteFunction(
      key, callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnClear(
    int idb_object_store_id,
    int32 response_id,
    int32 transaction_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id));
  idb_object_store->clear(callbacks.release(), *idb_transaction, *ec);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCreateIndex(
   const IndexedDBHostMsg_ObjectStoreCreateIndex_Params& params,
   int32* index_id, WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_object_store || !idb_transaction)
    return;

  *ec = 0;
  WebIDBIndex* index = idb_object_store->createIndex(
      params.name, params.key_path, params.unique, *idb_transaction, *ec);
  *index_id = *ec ? 0 : parent_->Add(index);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.idb_object_store_id);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id);
  if (!idb_transaction || !idb_object_store)
    return;

  *ec = 0;
  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id));
  idb_object_store->openCursor(
      WebIDBKeyRange(params.lower_key, params.upper_key, params.lower_open,
                     params.upper_open),
      params.direction, callbacks.release(), *idb_transaction, *ec);
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
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorKey, OnKey)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorValue, OnValue)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorUpdate, OnUpdate)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorContinue, OnContinue)
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

void IndexedDBDispatcherHost::CursorDispatcherHost::OnValue(
    int32 object_id,
    SerializedScriptValue* script_value,
    IndexedDBKey* key) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, object_id);
  if (!idb_cursor)
    return;

  WebSerializedScriptValue temp_script_value;
  WebIDBKey temp_key;
  idb_cursor->value(temp_script_value, temp_key);

  *script_value = SerializedScriptValue(temp_script_value);
  *key = IndexedDBKey(temp_key);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnUpdate(
    int32 cursor_id,
    int32 response_id,
    const SerializedScriptValue& value,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->update(
      value, new IndexedDBCallbacks<WebIDBKey>(parent_, response_id), *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnContinue(
    int32 cursor_id,
    int32 response_id,
    const IndexedDBKey& key,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  idb_cursor->continueFunction(
      key, new IndexedDBCallbacks<WebIDBCursor>(parent_, response_id), *ec);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDelete(
    int32 cursor_id,
    int32 response_id,
    WebKit::WebExceptionCode* ec) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(&map_, cursor_id);
  if (!idb_cursor)
    return;

  *ec = 0;
  // TODO(jorlow): This should be delete.
  idb_cursor->remove(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id), *ec);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id);
  if (!idb_transaction)
    return;

  idb_transaction->didCompleteTaskEvents();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id);
}
