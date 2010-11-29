// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "ipc/ipc_message.h"

class HostContentSettingsMap;
class IndexedDBKey;
class Profile;
class SerializedScriptValue;
struct ViewHostMsg_IDBDatabaseCreateObjectStore_Params;
struct ViewHostMsg_IDBFactoryOpen_Params;
struct ViewHostMsg_IDBIndexOpenCursor_Params;
struct ViewHostMsg_IDBObjectStoreCreateIndex_Params;
struct ViewHostMsg_IDBObjectStoreOpenCursor_Params;
struct ViewHostMsg_IDBObjectStorePut_Params;

namespace WebKit {
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBIndex;
class WebIDBObjectStore;
class WebIDBTransaction;
}

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost
    : public base::RefCountedThreadSafe<IndexedDBDispatcherHost> {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(IPC::Message::Sender* sender, Profile* profile);

  // Only call from ResourceMessageFilter on the IO thread.
  void Init(int process_id, base::ProcessHandle process_handle);

  // Only call from ResourceMessageFilter on the IO thread.  Calls self on the
  // WebKit thread in some cases.
  void Shutdown();

  // Only call from ResourceMessageFilter on the IO thread.
  bool OnMessageReceived(const IPC::Message& message);

  // Send a message to the renderer process associated with our sender_ via the
  // IO thread.  May be called from any thread.
  void Send(IPC::Message* message);

  // A shortcut for accessing our context.
  IndexedDBContext* Context() {
    return webkit_context_->indexed_db_context();
  }

  // The various IndexedDBCallbacks children call these methods to add the
  // results into the applicable map.  See below for more details.
  int32 Add(WebKit::WebIDBCursor* idb_cursor);
  int32 Add(WebKit::WebIDBDatabase* idb_database);
  int32 Add(WebKit::WebIDBIndex* idb_index);
  int32 Add(WebKit::WebIDBObjectStore* idb_object_store);
  int32 Add(WebKit::WebIDBTransaction* idb_transaction);

 private:
  friend class base::RefCountedThreadSafe<IndexedDBDispatcherHost>;
  ~IndexedDBDispatcherHost();

  // Message processing. Most of the work is delegated to the dispatcher hosts
  // below.
  void OnMessageReceivedWebKit(const IPC::Message& message);
  void OnIDBFactoryOpen(const ViewHostMsg_IDBFactoryOpen_Params& p);

  // Helper templates.
  template <class ReturnType>
  ReturnType* GetOrTerminateProcess(
    IDMap<ReturnType, IDMapOwnPointer>* map, int32 return_object_id,
    IPC::Message* reply_msg, uint32 message_type);

  template <typename ReplyType, typename MessageType,
            typename WebObjectType, typename Method>
  void SyncGetter(IDMap<WebObjectType, IDMapOwnPointer>* map, int32 object_id,
                  IPC::Message* reply_msg, Method method);

  template <typename ObjectType>
  void DestroyObject(IDMap<ObjectType, IDMapOwnPointer>* map, int32 object_id,
                     uint32 message_type);

  class DatabaseDispatcherHost {
   public:
    explicit DatabaseDispatcherHost(IndexedDBDispatcherHost* parent);
    ~DatabaseDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnName(int32 idb_database_id, IPC::Message* reply_msg);
    void OnVersion(int32 idb_database_id, IPC::Message* reply_msg);
    void OnObjectStoreNames(int32 idb_database_id, IPC::Message* reply_msg);
    void OnCreateObjectStore(
        const ViewHostMsg_IDBDatabaseCreateObjectStore_Params& params,
        IPC::Message* reply_msg);
    void OnRemoveObjectStore(int32 idb_database_id,
                             const string16& name,
                             int32 transaction_id,
                             IPC::Message* reply_msg);
    void OnSetVersion(int32 idb_database_id,
                      int32 response_id,
                      const string16& version,
                      IPC::Message* reply_msg);
    void OnTransaction(int32 idb_database_id,
                       const std::vector<string16>& names,
                       int32 mode, int32 timeout,
                       IPC::Message* reply_msg);
    void OnDestroyed(int32 idb_database_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBDatabase, IDMapOwnPointer> map_;
  };

  class IndexDispatcherHost {
   public:
    explicit IndexDispatcherHost(IndexedDBDispatcherHost* parent);
    ~IndexDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnName(int32 idb_index_id, IPC::Message* reply_msg);
    void OnStoreName(int32 idb_index_id, IPC::Message* reply_msg);
    void OnKeyPath(int32 idb_index_id, IPC::Message* reply_msg);
    void OnUnique(int32 idb_index_id, IPC::Message* reply_msg);
    void OnOpenObjectCursor(
        const ViewHostMsg_IDBIndexOpenCursor_Params& params,
        IPC::Message* reply_msg);
    void OnOpenKeyCursor(const ViewHostMsg_IDBIndexOpenCursor_Params& params,
                         IPC::Message* reply_msg);
    void OnGetObject(int idb_index_id,
                     int32 response_id,
                     const IndexedDBKey& key,
                     int32 transaction_id,
                     IPC::Message* reply_msg);
    void OnGetKey(int idb_index_id,
                  int32 response_id,
                  const IndexedDBKey& key,
                  int32 transaction_id,
                  IPC::Message* reply_msg);
    void OnDestroyed(int32 idb_index_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBIndex, IDMapOwnPointer> map_;
  };

  class ObjectStoreDispatcherHost {
   public:
    explicit ObjectStoreDispatcherHost(IndexedDBDispatcherHost* parent);
    ~ObjectStoreDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnName(int32 idb_object_store_id, IPC::Message* reply_msg);
    void OnKeyPath(int32 idb_object_store_id, IPC::Message* reply_msg);
    void OnIndexNames(int32 idb_object_store_id, IPC::Message* reply_msg);
    void OnGet(int idb_object_store_id,
               int32 response_id,
               const IndexedDBKey& key,
               int32 transaction_id,
               IPC::Message* reply_msg);
    void OnPut(const ViewHostMsg_IDBObjectStorePut_Params& params,
               IPC::Message* reply_msg);
    void OnRemove(int idb_object_store_id,
                  int32 response_id,
                  const IndexedDBKey& key,
                  int32 transaction_id,
                  IPC::Message* reply_msg);
    void OnCreateIndex(
        const ViewHostMsg_IDBObjectStoreCreateIndex_Params& params,
        IPC::Message* reply_msg);
    void OnIndex(int32 idb_object_store_id,
                 const string16& name,
                 IPC::Message* reply_msg);
    void OnRemoveIndex(int32 idb_object_store_id,
                       const string16& name,
                       int32 transaction_id,
                       IPC::Message* reply_msg);
    void OnOpenCursor(
        const ViewHostMsg_IDBObjectStoreOpenCursor_Params& params,
        IPC::Message* reply_msg);
    void OnDestroyed(int32 idb_object_store_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBObjectStore, IDMapOwnPointer> map_;
  };

  class CursorDispatcherHost {
   public:
    explicit CursorDispatcherHost(IndexedDBDispatcherHost* parent);
    ~CursorDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnDirection(int32 idb_object_store_id, IPC::Message* reply_msg);
    void OnKey(int32 idb_object_store_id, IPC::Message* reply_msg);
    void OnValue(int32 idb_object_store_id, IPC::Message* reply_msg);
    void OnUpdate(int32 idb_object_store_id,
                  int32 response_id,
                  const SerializedScriptValue& value,
                  IPC::Message* reply_msg);
    void OnContinue(int32 idb_object_store_id,
                    int32 response_id,
                    const IndexedDBKey& key,
                    IPC::Message* reply_msg);
    void OnRemove(int32 idb_object_store_id,
                  int32 response_id,
                  IPC::Message* reply_msg);
    void OnDestroyed(int32 idb_cursor_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBCursor, IDMapOwnPointer> map_;
  };

  class TransactionDispatcherHost {
   public:
    explicit TransactionDispatcherHost(IndexedDBDispatcherHost* parent);
    ~TransactionDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    // TODO: add the rest of the transaction methods.
    void OnAbort(int32 transaction_id);
    void OnMode(int32 transaction_id, IPC::Message* reply_msg);
    void OnObjectStore(int32 transaction_id,
                       const string16& name,
                       IPC::Message* reply_msg);
    void OnDidCompleteTaskEvents(int transaction_id);
    void OnDestroyed(int32 idb_transaction_id);

    IndexedDBDispatcherHost* parent_;
    typedef IDMap<WebKit::WebIDBTransaction, IDMapOwnPointer> MapType;
    MapType map_;
  };

  // Only use on the IO thread.
  IPC::Message::Sender* sender_;

  // Data shared between renderer processes with the same profile.
  scoped_refptr<WebKitContext> webkit_context_;

  // Tells us whether the user wants to allow databases to be opened.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Only access on WebKit thread.
  scoped_ptr<DatabaseDispatcherHost> database_dispatcher_host_;
  scoped_ptr<IndexDispatcherHost> index_dispatcher_host_;
  scoped_ptr<ObjectStoreDispatcherHost> object_store_dispatcher_host_;
  scoped_ptr<CursorDispatcherHost> cursor_dispatcher_host_;
  scoped_ptr<TransactionDispatcherHost> transaction_dispatcher_host_;

  // If we get a corrupt message from a renderer, we need to kill it using this
  // handle.
  base::ProcessHandle process_handle_;

  // Used to dispatch messages to the correct view host.
  int process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
