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
  bool OnMessageReceived(const IPC::Message& message, bool* msg_is_ok);

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

  // IndexedDB message handlers.
  void OnIndexedDatabaseOpen(
      const ViewHostMsg_IndexedDatabaseOpen_Params& params);
  void OnIDBDatabaseDestroyed(int32 idb_database_id);
  void OnIDBDatabaseName(int32 idb_database_id, IPC::Message* reply_msg);
  void OnIDBDatabaseDescription(int32 idb_database_id, IPC::Message* reply_msg);
  void OnIDBDatabaseVersion(int32 idb_database_id, IPC::Message* reply_msg);
  void OnIDBDatabaseObjectStores(int32 idb_database_id,
                                 IPC::Message* reply_msg);

  WebKit::WebIDBDatabase* GetDatabaseOrTerminateProcess(
      int32 idb_database_id, IPC::Message* reply_msg);

  // Only use on the IO thread.
  IPC::Message::Sender* sender_;

  // Data shared between renderer processes with the same profile.
  scoped_refptr<WebKitContext> webkit_context_;

  // Maps from IDs we pass to the renderer and the actual WebKit objects.
  // The map takes ownership and returns an ID.  That ID is passed to the
  // renderer and used to reference it.  All access must be on WebKit thread.
  scoped_ptr<IDMap<WebKit::WebIDBDatabase, IDMapOwnPointer> >
      idb_database_map_;
  // TODO(andreip/jorlow): Add other maps here.

  // If we get a corrupt message from a renderer, we need to kill it using this
  // handle.
  base::ProcessHandle process_handle_;

  // Used to dispatch messages to the correct view host.
  int process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
