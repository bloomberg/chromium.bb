// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/indexed_db_callbacks.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIndexedDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMStringList.h"

using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebSecurityOrigin;
using WebKit::WebDOMStringList;

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    IPC::Message::Sender* sender, WebKitContext* webkit_context)
    : sender_(sender),
      webkit_context_(webkit_context),
      idb_database_map_(new IDMap<WebKit::WebIDBDatabase, IDMapOwnPointer>()),
      process_handle_(0) {
  DCHECK(sender_);
  DCHECK(webkit_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
}

void IndexedDBDispatcherHost::Init(int process_id,
                                   base::ProcessHandle process_handle) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(sender_);  // Ensure Shutdown() has not been called.
  DCHECK(!process_handle_);  // Make sure Init() has not yet been called.
  DCHECK(process_handle);
  process_id_ = process_id;
  process_handle_ = process_handle;
}

void IndexedDBDispatcherHost::Shutdown() {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    sender_ = NULL;

    bool success = ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &IndexedDBDispatcherHost::Shutdown));
    if (success)
      return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT) ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  DCHECK(!sender_);

  idb_database_map_.reset();
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                bool* msg_is_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(process_handle_);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IndexedDatabaseOpen, OnIndexedDatabaseOpen)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseName,
                                    OnIDBDatabaseName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseDescription,
                                    OnIDBDatabaseDescription)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseVersion,
                                    OnIDBDatabaseVersion)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseObjectStores,
                                    OnIDBDatabaseObjectStores)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBDatabaseDestroyed,
                        OnIDBDatabaseDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::Send(IPC::Message* message) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    // TODO(jorlow): Even if we successfully post, I believe it's possible for
    //               the task to never run (if the IO thread is already shutting
    //               down).  We may want to handle this case, though
    //               realistically it probably doesn't matter.
    if (!ChromeThread::PostTask(
            ChromeThread::IO, FROM_HERE, NewRunnableMethod(
                this, &IndexedDBDispatcherHost::Send, message))) {
      // The IO thread is dead.
      delete message;
    }
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!sender_)
    delete message;
  else
    sender_->Send(message);
}

int32 IndexedDBDispatcherHost::AddIDBDatabase(WebIDBDatabase* idb_database) {
  return idb_database_map_->Add(idb_database);
}

class IndexedDatabaseOpenCallbacks : public IndexedDBCallbacks {
 public:
  IndexedDatabaseOpenCallbacks(IndexedDBDispatcherHost* parent,
                               int32 response_id)
      : IndexedDBCallbacks(parent, response_id) {
  }

  virtual void onError(const WebIDBDatabaseError& error) {
    parent()->Send(new ViewMsg_IndexedDatabaseOpenError(
        response_id(), error.code(), error.message()));
  }

  virtual void onSuccess(WebIDBDatabase* idb_database) {
    int32 idb_database_id = parent()->AddIDBDatabase(idb_database);
    parent()->Send(new ViewMsg_IndexedDatabaseOpenSuccess(response_id(),
                                                          idb_database_id));
  }
};

void IndexedDBDispatcherHost::OnIndexedDatabaseOpen(
    const ViewHostMsg_IndexedDatabaseOpen_Params& params) {
  // TODO(jorlow): Check the content settings map and use params.routing_id_
  //               if it's necessary to ask the user for permission.

  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &IndexedDBDispatcherHost::OnIndexedDatabaseOpen, params));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  int exception_code = 0;
  Context()->GetIndexedDatabase()->open(
      params.name_, params.description_,
      new IndexedDatabaseOpenCallbacks(this, params.response_id_),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin_), NULL,
      exception_code);
  // ViewHostMsg_IndexedDatabaseOpen is async because we assume the exception
  // code is always 0 in the renderer side.
  DCHECK(exception_code == 0);
}

void IndexedDBDispatcherHost::OnIDBDatabaseName(
    int32 idb_database_id, IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &IndexedDBDispatcherHost::OnIDBDatabaseName,
        idb_database_id, reply_msg));
    return;
  }

  WebIDBDatabase* idb_database = GetDatabaseOrTerminateProcess(
      idb_database_id, reply_msg);
  if (!idb_database)
      return;

  const string16& name = idb_database->name();
  ViewHostMsg_IDBDatabaseName::WriteReplyParams(reply_msg, name);
  Send(reply_msg);
}

void IndexedDBDispatcherHost::OnIDBDatabaseDescription(
    int32 idb_database_id, IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &IndexedDBDispatcherHost::OnIDBDatabaseDescription,
        idb_database_id, reply_msg));
    return;
  }

  WebIDBDatabase* idb_database = GetDatabaseOrTerminateProcess(
      idb_database_id, reply_msg);
  if (!idb_database)
    return;

  const string16& description = idb_database->description();
  ViewHostMsg_IDBDatabaseDescription::WriteReplyParams(reply_msg, description);
  Send(reply_msg);
}

void IndexedDBDispatcherHost::OnIDBDatabaseVersion(
    int32 idb_database_id, IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &IndexedDBDispatcherHost::OnIDBDatabaseVersion,
        idb_database_id, reply_msg));
    return;
  }

  WebIDBDatabase* idb_database = GetDatabaseOrTerminateProcess(
      idb_database_id, reply_msg);
  if (!idb_database)
    return;

  const string16& version = idb_database->version();
  ViewHostMsg_IDBDatabaseVersion::WriteReplyParams(reply_msg, version);
  Send(reply_msg);
}

void IndexedDBDispatcherHost::OnIDBDatabaseObjectStores(
    int32 idb_database_id, IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &IndexedDBDispatcherHost::OnIDBDatabaseObjectStores,
        idb_database_id, reply_msg));
    return;
  }

  WebIDBDatabase* idb_database = GetDatabaseOrTerminateProcess(
      idb_database_id, reply_msg);
  if (!idb_database)
    return;

  WebDOMStringList web_object_stores = idb_database->objectStores();
  std::vector<string16> object_stores;
  for (unsigned i = 0; i < web_object_stores.length(); ++i)
    object_stores[i] = web_object_stores.item(i);
  ViewHostMsg_IDBDatabaseObjectStores::WriteReplyParams(reply_msg,
                                                        object_stores);
  Send(reply_msg);
}

void IndexedDBDispatcherHost::OnIDBDatabaseDestroyed(int32 idb_database_id) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &IndexedDBDispatcherHost::OnIDBDatabaseDestroyed,
        idb_database_id));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  idb_database_map_->Remove(idb_database_id);
}

WebIDBDatabase* IndexedDBDispatcherHost::GetDatabaseOrTerminateProcess(
    int32 idb_database_id, IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBDatabase* idb_database = idb_database_map_->Lookup(idb_database_id);
  if (!idb_database) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageGetItem::ID, process_handle_);
    delete reply_msg;
  }
  return idb_database;
}
