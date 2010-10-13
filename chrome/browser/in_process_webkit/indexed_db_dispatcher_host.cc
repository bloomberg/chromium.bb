// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/in_process_webkit/indexed_db_callbacks.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/serialized_script_value.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBIndex.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
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
static const uint64 kDatabaseQuota = 5 * 1024 * 1024;

template <class T>
void DeleteOnWebKitThread(T* obj) {
  if (!BrowserThread::DeleteSoon(BrowserThread::WEBKIT, FROM_HERE, obj))
    delete obj;
}

}

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    IPC::Message::Sender* sender, Profile* profile)
    : sender_(sender),
      webkit_context_(profile->GetWebKitContext()),
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
      process_handle_(0) {
  DCHECK(sender_);
  DCHECK(webkit_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  DeleteOnWebKitThread(database_dispatcher_host_.release());
  DeleteOnWebKitThread(index_dispatcher_host_.release());
  DeleteOnWebKitThread(object_store_dispatcher_host_.release());
  DeleteOnWebKitThread(cursor_dispatcher_host_.release());
  DeleteOnWebKitThread(transaction_dispatcher_host_.release());
}

void IndexedDBDispatcherHost::Init(int process_id,
                                   base::ProcessHandle process_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(sender_);  // Ensure Shutdown() has not been called.
  DCHECK(!process_handle_);  // Make sure Init() has not yet been called.
  DCHECK(process_handle);
  process_id_ = process_id;
  process_handle_ = process_handle;
}

void IndexedDBDispatcherHost::Shutdown() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    sender_ = NULL;

    bool success = BrowserThread::PostTask(
        BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &IndexedDBDispatcherHost::Shutdown));
    if (success)
      return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT) ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  DCHECK(!sender_);

  database_dispatcher_host_.reset();
  index_dispatcher_host_.reset();
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(process_handle_);

  switch (message.type()) {
    case ViewHostMsg_IDBCursorDirection::ID:
    case ViewHostMsg_IDBCursorKey::ID:
    case ViewHostMsg_IDBCursorValue::ID:
    case ViewHostMsg_IDBCursorUpdate::ID:
    case ViewHostMsg_IDBCursorContinue::ID:
    case ViewHostMsg_IDBCursorRemove::ID:
    case ViewHostMsg_IDBCursorDestroyed::ID:
    case ViewHostMsg_IDBFactoryOpen::ID:
    case ViewHostMsg_IDBDatabaseName::ID:
    case ViewHostMsg_IDBDatabaseDescription::ID:
    case ViewHostMsg_IDBDatabaseVersion::ID:
    case ViewHostMsg_IDBDatabaseObjectStores::ID:
    case ViewHostMsg_IDBDatabaseCreateObjectStore::ID:
    case ViewHostMsg_IDBDatabaseRemoveObjectStore::ID:
    case ViewHostMsg_IDBDatabaseSetVersion::ID:
    case ViewHostMsg_IDBDatabaseTransaction::ID:
    case ViewHostMsg_IDBDatabaseDestroyed::ID:
    case ViewHostMsg_IDBIndexName::ID:
    case ViewHostMsg_IDBIndexStoreName::ID:
    case ViewHostMsg_IDBIndexKeyPath::ID:
    case ViewHostMsg_IDBIndexUnique::ID:
    case ViewHostMsg_IDBIndexDestroyed::ID:
    case ViewHostMsg_IDBIndexOpenObjectCursor::ID:
    case ViewHostMsg_IDBIndexOpenKeyCursor::ID:
    case ViewHostMsg_IDBIndexGetObject::ID:
    case ViewHostMsg_IDBIndexGetKey::ID:
    case ViewHostMsg_IDBObjectStoreName::ID:
    case ViewHostMsg_IDBObjectStoreKeyPath::ID:
    case ViewHostMsg_IDBObjectStoreIndexNames::ID:
    case ViewHostMsg_IDBObjectStoreGet::ID:
    case ViewHostMsg_IDBObjectStorePut::ID:
    case ViewHostMsg_IDBObjectStoreRemove::ID:
    case ViewHostMsg_IDBObjectStoreCreateIndex::ID:
    case ViewHostMsg_IDBObjectStoreIndex::ID:
    case ViewHostMsg_IDBObjectStoreRemoveIndex::ID:
    case ViewHostMsg_IDBObjectStoreOpenCursor::ID:
    case ViewHostMsg_IDBObjectStoreDestroyed::ID:
    case ViewHostMsg_IDBTransactionAbort::ID:
    case ViewHostMsg_IDBTransactionMode::ID:
    case ViewHostMsg_IDBTransactionDestroyed::ID:
    case ViewHostMsg_IDBTransactionDidCompleteTaskEvents::ID:
    case ViewHostMsg_IDBTransactionObjectStore::ID:
      break;
    default:
      return false;
  }

  bool success = BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
          this, &IndexedDBDispatcherHost::OnMessageReceivedWebKit, message));
  DCHECK(success);
  return true;
}

void IndexedDBDispatcherHost::Send(IPC::Message* message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // TODO(jorlow): Even if we successfully post, I believe it's possible for
    //               the task to never run (if the IO thread is already shutting
    //               down).  We may want to handle this case, though
    //               realistically it probably doesn't matter.
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE, NewRunnableMethod(
                this, &IndexedDBDispatcherHost::Send, message))) {
      // The IO thread is dead.
      delete message;
    }
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!sender_)
    delete message;
  else
    sender_->Send(message);
}

void IndexedDBDispatcherHost::OnMessageReceivedWebKit(
    const IPC::Message& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DCHECK(process_handle_);

  bool msg_is_ok = true;
  bool handled =
      database_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      index_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      object_store_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      cursor_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      transaction_dispatcher_host_->OnMessageReceived(message, &msg_is_ok);

  if (!handled) {
    handled = true;
    DCHECK(msg_is_ok);
    IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost, message, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_IDBFactoryOpen,
                          OnIDBFactoryOpen)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }

  DCHECK(handled);
  if (!msg_is_ok) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(message.type(),
                                                         process_handle_);
  }
}

int32 IndexedDBDispatcherHost::Add(WebIDBCursor* idb_cursor) {
  return cursor_dispatcher_host_->map_.Add(idb_cursor);
}

int32 IndexedDBDispatcherHost::Add(WebIDBDatabase* idb_database) {
  return database_dispatcher_host_->map_.Add(idb_database);
}

int32 IndexedDBDispatcherHost::Add(WebIDBIndex* idb_index) {
  if (!idb_index)
    return 0;
  return index_dispatcher_host_->map_.Add(idb_index);
}

int32 IndexedDBDispatcherHost::Add(WebIDBObjectStore* idb_object_store) {
  if (!idb_object_store)
    return 0;
  return object_store_dispatcher_host_->map_.Add(idb_object_store);
}

int32 IndexedDBDispatcherHost::Add(WebIDBTransaction* idb_transaction) {
  int32 id = transaction_dispatcher_host_->map_.Add(idb_transaction);
  idb_transaction->setCallbacks(new IndexedDBTransactionCallbacks(this, id));
  return id;
}

void IndexedDBDispatcherHost::OnIDBFactoryOpen(
    const ViewHostMsg_IDBFactoryOpen_Params& params) {
  FilePath base_path = webkit_context_->data_path();
  FilePath indexed_db_path;
  if (!base_path.empty()) {
    indexed_db_path = base_path.Append(
        IndexedDBContext::kIndexedDBDirectory);
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  GURL host(string16(WebSecurityOrigin::createFromDatabaseIdentifier(
      params.origin_).toString()));

  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          host, CONTENT_SETTINGS_TYPE_COOKIES, "");

  CallRenderViewHostContentSettingsDelegate(
      process_id_, params.routing_id_,
      &RenderViewHostDelegate::ContentSettings::OnIndexedDBAccessed,
      host, params.name_, params.description_,
      content_setting == CONTENT_SETTING_BLOCK);

  if (content_setting == CONTENT_SETTING_BLOCK) {
    // TODO(jorlow): Change this to the proper error code once we figure out
    // one.
    int error_code = 0; // Defined by the IndexedDB spec.
    static string16 error_message = ASCIIToUTF16(
        "The user denied permission to open the database.");
    Send(new ViewMsg_IDBCallbacksError(params.response_id_, error_code,
                                       error_message));
    return;
  }

  DCHECK(kDatabaseQuota == params.maximum_size_);
  Context()->GetIDBFactory()->open(
      params.name_, params.description_,
      new IndexedDBCallbacks<WebIDBDatabase>(this, params.response_id_),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin_), NULL,
      webkit_glue::FilePathToWebString(indexed_db_path), kDatabaseQuota);
}

//////////////////////////////////////////////////////////////////////
// Helper templates.
//

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 return_object_id,
    IPC::Message* reply_msg, uint32 message_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  ObjectType* return_object = map->Lookup(return_object_id);
  if (!return_object) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(message_type,
                                                         process_handle_);
    delete reply_msg;
  }
  return return_object;
}

template <typename ReplyType, typename MessageType,
          typename MapObjectType, typename Method>
void IndexedDBDispatcherHost::SyncGetter(
    IDMap<MapObjectType, IDMapOwnPointer>* map, int32 object_id,
    IPC::Message* reply_msg, Method method) {
  MapObjectType* object = GetOrTerminateProcess(map, object_id, reply_msg,
                                                MessageType::ID);
  if (!object)
      return;

  ReplyType reply = (object->*method)();
  MessageType::WriteReplyParams(reply_msg, reply);
  Send(reply_msg);
}

template <typename ObjectType>
void IndexedDBDispatcherHost::DestroyObject(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 object_id,
    uint32 message_type) {
  GetOrTerminateProcess(map, object_id, NULL, message_type);
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseName, OnName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseDescription,
                                    OnDescription)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseVersion, OnVersion)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseObjectStores,
                                    OnObjectStores)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseCreateObjectStore,
                                    OnCreateObjectStore)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseRemoveObjectStore,
                                    OnRemoveObjectStore)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseSetVersion,
                                    OnSetVersion)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseTransaction,
                                    OnTransaction)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBDatabaseDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBDatabaseName>(
      &map_, object_id, reply_msg, &WebIDBDatabase::name);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDescription(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBDatabaseDescription>(
      &map_, object_id, reply_msg, &WebIDBDatabase::description);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnVersion(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBDatabaseVersion>(
      &map_, object_id, reply_msg, &WebIDBDatabase::version);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnObjectStores(
    int32 idb_database_id, IPC::Message* reply_msg) {
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, reply_msg,
      ViewHostMsg_IDBDatabaseObjectStores::ID);
  if (!idb_database)
    return;

  WebDOMStringList web_object_stores = idb_database->objectStores();
  std::vector<string16> object_stores;
  object_stores.reserve(web_object_stores.length());
  for (unsigned i = 0; i < web_object_stores.length(); ++i)
    object_stores.push_back(web_object_stores.item(i));
  ViewHostMsg_IDBDatabaseObjectStores::WriteReplyParams(reply_msg,
                                                        object_stores);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateObjectStore(
    const ViewHostMsg_IDBDatabaseCreateObjectStore_Params& params,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, params.idb_database_id_, NULL,
      ViewHostMsg_IDBDatabaseCreateObjectStore::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id_,
      NULL, ViewHostMsg_IDBDatabaseCreateObjectStore::ID);
  if (!idb_database || !idb_transaction)
    return;

  WebExceptionCode ec = 0;
  WebIDBObjectStore* object_store = idb_database->createObjectStore(
      params.name_, params.key_path_, params.auto_increment_,
      *idb_transaction, ec);
  ViewHostMsg_IDBDatabaseCreateObjectStore::WriteReplyParams(
      reply_msg, ec ? 0 : parent_->Add(object_store), ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnRemoveObjectStore(
    int32 idb_database_id,
    const string16& name,
    int32 transaction_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, NULL,
      ViewHostMsg_IDBDatabaseRemoveObjectStore::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id, NULL,
      ViewHostMsg_IDBDatabaseRemoveObjectStore::ID);
  if (!idb_database || !idb_transaction)
    return;

  WebExceptionCode ec = 0;
  idb_database->removeObjectStore(name, *idb_transaction, ec);
  ViewHostMsg_IDBDatabaseRemoveObjectStore::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetVersion(
    int32 idb_database_id,
    int32 response_id,
    const string16& version,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, NULL,
      ViewHostMsg_IDBDatabaseSetVersion::ID);
  if (!idb_database)
    return;

  WebExceptionCode ec = 0;
  idb_database->setVersion(
      version,
      new IndexedDBCallbacks<WebIDBTransaction>(parent_, response_id),
      ec);
  ViewHostMsg_IDBDatabaseSetVersion::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnTransaction(
    int32 idb_database_id,
    const std::vector<string16>& names,
    int32 mode,
    int32 timeout,
    IPC::Message* reply_msg) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, reply_msg,
      ViewHostMsg_IDBDatabaseTransaction::ID);
  if (!database)
      return;

  WebDOMStringList object_stores;
  for (std::vector<string16>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    object_stores.append(*it);
  }

  WebExceptionCode ec = 0;
  WebIDBTransaction* transaction = database->transaction(
      object_stores, mode, timeout, ec);
  DCHECK(!transaction != !ec);
  int32 id = ec ? 0 : parent_->Add(transaction);
  ViewHostMsg_IDBDatabaseTransaction::WriteReplyParams(reply_msg, id, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id,
                         ViewHostMsg_IDBDatabaseDestroyed::ID);
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexName, OnName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexStoreName, OnStoreName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexKeyPath, OnKeyPath)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexUnique, OnUnique)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexOpenObjectCursor,
                                    OnOpenObjectCursor)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexOpenKeyCursor,
                                    OnOpenKeyCursor)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexGetObject, OnGetObject)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexGetKey, OnGetKey)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBIndexDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::IndexDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBIndexName>(
      &map_, object_id, reply_msg, &WebIDBIndex::name);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnStoreName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBIndexStoreName>(
      &map_, object_id, reply_msg, &WebIDBIndex::storeName);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnKeyPath(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<NullableString16, ViewHostMsg_IDBIndexKeyPath>(
      &map_, object_id, reply_msg, &WebIDBIndex::keyPath);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnUnique(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<bool, ViewHostMsg_IDBIndexUnique>(
      &map_, object_id, reply_msg, &WebIDBIndex::unique);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenObjectCursor(
    const ViewHostMsg_IDBIndexOpenCursor_Params& params,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id_, NULL,
      ViewHostMsg_IDBIndexOpenObjectCursor::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_,
      params.transaction_id_, NULL, ViewHostMsg_IDBIndexOpenObjectCursor::ID);
  if (!idb_transaction || !idb_index)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id_));
  WebExceptionCode ec = 0;
  idb_index->openObjectCursor(
      WebIDBKeyRange(params.left_key_, params.right_key_, params.key_flags_),
      params.direction_, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBIndexOpenObjectCursor::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnOpenKeyCursor(
    const ViewHostMsg_IDBIndexOpenCursor_Params& params,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, params.idb_index_id_, NULL, ViewHostMsg_IDBIndexOpenKeyCursor::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id_,
      NULL, ViewHostMsg_IDBIndexOpenKeyCursor::ID);
  if (!idb_transaction || !idb_index)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id_));
  WebExceptionCode ec = 0;
  idb_index->openKeyCursor(
      WebIDBKeyRange(params.left_key_, params.right_key_, params.key_flags_),
      params.direction_, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBIndexOpenKeyCursor::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetObject(
    int idb_index_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, idb_index_id, NULL, ViewHostMsg_IDBIndexGetObject::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id, NULL,
      ViewHostMsg_IDBIndexGetObject::ID);
  if (!idb_transaction || !idb_index)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id));
  WebExceptionCode ec = 0;
  idb_index->getObject(key, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBIndexGetObject::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnGetKey(
    int idb_index_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBIndex* idb_index = parent_->GetOrTerminateProcess(
      &map_, idb_index_id, NULL, ViewHostMsg_IDBIndexGetKey::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id, NULL,
      ViewHostMsg_IDBIndexGetKey::ID);
  if (!idb_transaction || !idb_index)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, response_id));
  WebExceptionCode ec = 0;
  idb_index->getKey(key, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBIndexGetKey::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id, ViewHostMsg_IDBIndexDestroyed::ID);
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreName, OnName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreKeyPath,
                                    OnKeyPath)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreIndexNames,
                                    OnIndexNames)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreGet, OnGet);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStorePut, OnPut);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreRemove, OnRemove);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreCreateIndex,
                                    OnCreateIndex);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreIndex, OnIndex);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreRemoveIndex,
                                    OnRemoveIndex);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreOpenCursor,
                                    OnOpenCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBObjectStoreName>(
      &map_, object_id, reply_msg, &WebIDBObjectStore::name);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnKeyPath(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<NullableString16, ViewHostMsg_IDBObjectStoreKeyPath>(
      &map_, object_id, reply_msg, &WebIDBObjectStore::keyPath);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndexNames(
    int32 idb_object_store_id, IPC::Message* reply_msg) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, reply_msg,
      ViewHostMsg_IDBObjectStoreIndexNames::ID);
  if (!idb_object_store)
    return;

  WebDOMStringList web_index_names = idb_object_store->indexNames();
  std::vector<string16> index_names;
  index_names.reserve(web_index_names.length());
  for (unsigned i = 0; i < web_index_names.length(); ++i)
    index_names.push_back(web_index_names.item(i));
  ViewHostMsg_IDBObjectStoreIndexNames::WriteReplyParams(reply_msg,
                                                         index_names);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnGet(
    int idb_object_store_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL, ViewHostMsg_IDBObjectStoreGet::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id, NULL,
      ViewHostMsg_IDBObjectStoreGet::ID);
  if (!idb_transaction || !idb_object_store)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebSerializedScriptValue>(parent_, response_id));
  WebExceptionCode ec = 0;
  idb_object_store->get(key, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBObjectStoreGet::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnPut(
    const ViewHostMsg_IDBObjectStorePut_Params& params,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id_, NULL,
      ViewHostMsg_IDBObjectStorePut::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id_,
      NULL, ViewHostMsg_IDBObjectStorePut::ID);
  if (!idb_transaction || !idb_object_store)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBKey>(parent_, params.response_id_));
  WebExceptionCode ec = 0;
  idb_object_store->put(params.serialized_value_, params.key_, params.add_only_,
                        callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBObjectStorePut::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnRemove(
    int idb_object_store_id,
    int32 response_id,
    const IndexedDBKey& key,
    int32 transaction_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL, ViewHostMsg_IDBObjectStoreRemove::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id, NULL,
      ViewHostMsg_IDBObjectStoreRemove::ID);
  if (!idb_transaction || !idb_object_store)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<void>(parent_, response_id));
  WebExceptionCode ec = 0;
  idb_object_store->remove(key, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBObjectStoreRemove::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCreateIndex(
   const ViewHostMsg_IDBObjectStoreCreateIndex_Params& params,
   IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id_, NULL,
      ViewHostMsg_IDBObjectStoreCreateIndex::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id_,
      NULL, ViewHostMsg_IDBObjectStoreCreateIndex::ID);
  if (!idb_object_store || !idb_transaction)
    return;

  WebExceptionCode ec = 0;
  WebIDBIndex* index = idb_object_store->createIndex(
      params.name_, params.key_path_, params.unique_, *idb_transaction, ec);
  ViewHostMsg_IDBObjectStoreCreateIndex::WriteReplyParams(
      reply_msg, ec ? 0 : parent_->Add(index), ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndex(
    int32 idb_object_store_id,
    const string16& name,
    IPC::Message* reply_msg) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, reply_msg,
      ViewHostMsg_IDBObjectStoreIndex::ID);
  if (!idb_object_store)
    return;

  WebExceptionCode ec = 0;
  WebIDBIndex* index = idb_object_store->index(name, ec);
  int32 object_id = parent_->Add(index);
  ViewHostMsg_IDBObjectStoreIndex::WriteReplyParams(reply_msg, object_id, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnRemoveIndex(
    int32 idb_object_store_id,
    const string16& name,
    int32 transaction_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL,
      ViewHostMsg_IDBObjectStoreRemoveIndex::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, transaction_id, NULL,
      ViewHostMsg_IDBObjectStoreRemoveIndex::ID);
  if (!idb_object_store || !idb_transaction)
    return;

  WebExceptionCode ec = 0;
  idb_object_store->removeIndex(name, *idb_transaction, ec);
  ViewHostMsg_IDBObjectStoreRemoveIndex::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnOpenCursor(
    const ViewHostMsg_IDBObjectStoreOpenCursor_Params& params,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.idb_object_store_id_, NULL,
      ViewHostMsg_IDBObjectStoreOpenCursor::ID);
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &parent_->transaction_dispatcher_host_->map_, params.transaction_id_,
      NULL, ViewHostMsg_IDBObjectStoreOpenCursor::ID);
  if (!idb_transaction || !idb_object_store)
    return;

  scoped_ptr<WebIDBCallbacks> callbacks(
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id_));
  WebExceptionCode ec = 0;
  idb_object_store->openCursor(
      WebIDBKeyRange(params.left_key_, params.right_key_, params.flags_),
      params.direction_, callbacks.release(), *idb_transaction, ec);
  ViewHostMsg_IDBObjectStoreOpenCursor::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(
      &map_, object_id, ViewHostMsg_IDBObjectStoreDestroyed::ID);
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorDirection,
                                    OnDirection)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorKey, OnKey)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorValue, OnValue)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorUpdate, OnUpdate)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorContinue, OnContinue)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorRemove, OnRemove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBCursorDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}


void IndexedDBDispatcherHost::CursorDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDirection(
    int32 object_id, IPC::Message* reply_msg) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, object_id, reply_msg,
      ViewHostMsg_IDBCursorDirection::ID);
  if (!idb_cursor)
    return;

  int direction = idb_cursor->direction();
  ViewHostMsg_IDBCursorDirection::WriteReplyParams(reply_msg, direction);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnKey(
    int32 object_id, IPC::Message* reply_msg) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, object_id, reply_msg,
      ViewHostMsg_IDBCursorKey::ID);
  if (!idb_cursor)
    return;

  IndexedDBKey key(idb_cursor->key());
  ViewHostMsg_IDBCursorKey::WriteReplyParams(reply_msg, key);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnValue(
    int32 object_id, IPC::Message* reply_msg) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, object_id, reply_msg,
      ViewHostMsg_IDBCursorValue::ID);
  if (!idb_cursor)
    return;

  WebSerializedScriptValue scriptValue;
  WebIDBKey key;
  idb_cursor->value(scriptValue, key);
  ViewHostMsg_IDBCursorValue::WriteReplyParams(
      reply_msg, SerializedScriptValue(scriptValue), IndexedDBKey(key));
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnUpdate(
    int32 cursor_id,
    int32 response_id,
    const SerializedScriptValue& value,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, cursor_id, NULL, ViewHostMsg_IDBCursorUpdate::ID);
  if (!idb_cursor)
    return;

  WebExceptionCode ec = 0;
  idb_cursor->update(
      value, new IndexedDBCallbacks<void>(parent_, response_id), ec);
  ViewHostMsg_IDBCursorUpdate::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnContinue(
    int32 cursor_id,
    int32 response_id,
    const IndexedDBKey& key,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, cursor_id, NULL, ViewHostMsg_IDBCursorContinue::ID);
  if (!idb_cursor)
    return;

  WebExceptionCode ec = 0;
  idb_cursor->continueFunction(
      key, new IndexedDBCallbacks<WebIDBCursor>(parent_, response_id), ec);
  ViewHostMsg_IDBCursorContinue::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnRemove(
    int32 cursor_id,
    int32 response_id,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, cursor_id, NULL, ViewHostMsg_IDBCursorUpdate::ID);
  if (!idb_cursor)
    return;

  WebExceptionCode ec = 0;
  idb_cursor->remove(new IndexedDBCallbacks<void>(parent_, response_id), ec);
  ViewHostMsg_IDBCursorUpdate::WriteReplyParams(reply_msg, ec);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(
      &map_, object_id, ViewHostMsg_IDBCursorDestroyed::ID);
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
}

bool IndexedDBDispatcherHost::TransactionDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::TransactionDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBTransactionAbort, OnAbort)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBTransactionMode, OnMode)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBTransactionObjectStore,
                                    OnObjectStore)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBTransactionDidCompleteTaskEvents,
                        OnDidCompleteTaskEvents)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBTransactionDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnAbort(
    int32 transaction_id) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id, 0, ViewHostMsg_IDBTransactionAbort::ID);
  if (!idb_transaction)
    return;

  idb_transaction->abort();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnMode(
    int32 transaction_id,
    IPC::Message* reply_msg) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id, 0, ViewHostMsg_IDBTransactionMode::ID);
  if (!idb_transaction)
    return;

  int mode = idb_transaction->mode();
  ViewHostMsg_IDBTransactionMode::WriteReplyParams(reply_msg, mode);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnObjectStore(
    int32 transaction_id, const string16& name, IPC::Message* reply_msg) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id, reply_msg,
      ViewHostMsg_IDBTransactionObjectStore::ID);
  if (!idb_transaction)
    return;

  WebIDBObjectStore* object_store = idb_transaction->objectStore(name);
  int32 object_id = object_store ? parent_->Add(object_store) : 0;
  ViewHostMsg_IDBTransactionObjectStore::WriteReplyParams(
      reply_msg, object_id);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::
    TransactionDispatcherHost::OnDidCompleteTaskEvents(int transaction_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id, 0,
      ViewHostMsg_IDBTransactionDidCompleteTaskEvents::ID);
  if (!idb_transaction)
    return;

  idb_transaction->didCompleteTaskEvents();
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(
      &map_, object_id, ViewHostMsg_IDBTransactionDestroyed::ID);
}
