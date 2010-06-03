// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "ipc/ipc_message.h"

struct ViewHostMsg_IndexedDatabaseOpen_Params;

namespace WebKit {
class WebIDBDatabase;
class WebIDBIndex;
}

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost
    : public base::RefCountedThreadSafe<IndexedDBDispatcherHost> {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(IPC::Message::Sender* sender,
                          WebKitContext* webkit_context);

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
  int32 AddIDBDatabase(WebKit::WebIDBDatabase* idb_database);
  // TODO(andreip/jorlow): Add functions for other maps here.

 private:
  friend class base::RefCountedThreadSafe<IndexedDBDispatcherHost>;
  ~IndexedDBDispatcherHost();

  // Message processing. Most of the work is delegated to the dispatcher hosts
  // below.
  void OnMessageReceivedWebKit(const IPC::Message& message);
  void OnIndexedDatabaseOpen(const ViewHostMsg_IndexedDatabaseOpen_Params& p);

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
    DatabaseDispatcherHost(IndexedDBDispatcherHost* parent);
    ~DatabaseDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnName(int32 idb_database_id, IPC::Message* reply_msg);
    void OnDescription(int32 idb_database_id, IPC::Message* reply_msg);
    void OnVersion(int32 idb_database_id, IPC::Message* reply_msg);
    void OnObjectStores(int32 idb_database_id, IPC::Message* reply_msg);
    void OnDestroyed(int32 idb_database_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBDatabase, IDMapOwnPointer> map_;
  };

  class IndexDispatcherHost {
   public:
    IndexDispatcherHost(IndexedDBDispatcherHost* parent);
    ~IndexDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnName(int32 idb_index_id, IPC::Message* reply_msg);
    void OnKeyPath(int32 idb_index_id, IPC::Message* reply_msg);
    void OnUnique(int32 idb_index_id, IPC::Message* reply_msg);
    void OnDestroyed(int32 idb_index_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBIndex, IDMapOwnPointer> map_;
  };

  // Only use on the IO thread.
  IPC::Message::Sender* sender_;

  // Data shared between renderer processes with the same profile.
  scoped_refptr<WebKitContext> webkit_context_;

  // Only access on WebKit thread.
  scoped_ptr<DatabaseDispatcherHost> database_dispatcher_host_;
  scoped_ptr<IndexDispatcherHost> index_dispatcher_host_;

  // If we get a corrupt message from a renderer, we need to kill it using this
  // handle.
  base::ProcessHandle process_handle_;

  // Used to dispatch messages to the correct view host.
  int process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
