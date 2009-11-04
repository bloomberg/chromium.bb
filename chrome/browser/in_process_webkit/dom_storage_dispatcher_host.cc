// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"

#include "base/nullable_string16.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/storage_area.h"
#include "chrome/browser/in_process_webkit/storage_namespace.h"
#include "chrome/browser/in_process_webkit/webkit_thread.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/render_messages.h"

DOMStorageDispatcherHost* DOMStorageDispatcherHost::storage_event_host_ = NULL;

DOMStorageDispatcherHost::
ScopedStorageEventContext::ScopedStorageEventContext(
    DOMStorageDispatcherHost* dispatcher_host) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(!storage_event_host_);
  storage_event_host_ = dispatcher_host;
  DCHECK(storage_event_host_);
}

DOMStorageDispatcherHost::
ScopedStorageEventContext::~ScopedStorageEventContext() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(storage_event_host_);
  storage_event_host_ = NULL;
}

DOMStorageDispatcherHost::DOMStorageDispatcherHost(
    IPC::Message::Sender* message_sender, WebKitContext* webkit_context,
    WebKitThread* webkit_thread)
    : webkit_context_(webkit_context),
      webkit_thread_(webkit_thread),
      message_sender_(message_sender),
      process_handle_(0) {
  DCHECK(webkit_context_.get());
  DCHECK(webkit_thread_);
  DCHECK(message_sender_);
}

DOMStorageDispatcherHost::~DOMStorageDispatcherHost() {
}

void DOMStorageDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(message_sender_);  // Make sure Shutdown() has not yet been called.
  DCHECK(!process_handle_);  // Make sure Init() has not yet been called.
  DCHECK(process_handle);
  Context()->RegisterDispatcherHost(this);
  process_handle_ = process_handle;
}

void DOMStorageDispatcherHost::Shutdown() {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    if (process_handle_)  // Init() was called
      Context()->UnregisterDispatcherHost(this);
    message_sender_ = NULL;

    // The task will only execute if the WebKit thread is already running.
    ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &DOMStorageDispatcherHost::Shutdown));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(!message_sender_);

  // TODO(jorlow): Do stuff that needs to be run on the WebKit thread.  Locks
  //               and others will likely need this, so let's not delete this
  //               code even though it doesn't do anyting yet.
}

/* static */
void DOMStorageDispatcherHost::DispatchStorageEvent(const NullableString16& key,
    const NullableString16& old_value, const NullableString16& new_value,
    const string16& origin, bool is_local_storage) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(is_local_storage);  // Only LocalStorage is implemented right now.
  DCHECK(storage_event_host_);
  ViewMsg_DOMStorageEvent_Params params;
  params.key_ = key;
  params.old_value_ = old_value;
  params.new_value_ = new_value;
  params.origin_ = origin;
  params.storage_type_ = is_local_storage ? DOM_STORAGE_LOCAL
                                          : DOM_STORAGE_SESSION;
  // The storage_event_host_ is the DOMStorageDispatcherHost that is up in the
  // current call stack since it caused the storage event to fire.
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(storage_event_host_,
          &DOMStorageDispatcherHost::OnStorageEvent, params));
}

bool DOMStorageDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                 bool *msg_is_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(process_handle_);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DOMStorageDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageNamespaceId,
                                    OnNamespaceId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageCloneNamespaceId,
                                    OnCloneNamespaceId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageStorageAreaId,
                                    OnStorageAreaId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageLength, OnLength)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageKey, OnKey)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageGetItem, OnGetItem)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageSetItem, OnSetItem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageRemoveItem, OnRemoveItem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageClear, OnClear)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DOMStorageDispatcherHost::Send(IPC::Message* message) {
  if (!message_sender_) {
    delete message;
    return;
  }

  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    message_sender_->Send(message);
    return;
  }

  // The IO thread can't go away while the WebKit thread is still running.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DOMStorageDispatcherHost::Send, message));
}

void DOMStorageDispatcherHost::OnNamespaceId(DOMStorageType storage_type,
                                             IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnNamespaceId,
        storage_type, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageNamespace* new_namespace;
  if (storage_type == DOM_STORAGE_LOCAL)
    new_namespace = Context()->LocalStorage();
  else
    new_namespace = Context()->NewSessionStorage();
  ViewHostMsg_DOMStorageNamespaceId::WriteReplyParams(reply_msg,
                                                      new_namespace->id());
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnCloneNamespaceId(int64 namespace_id,
                                                  IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnCloneNamespaceId,
        namespace_id, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageNamespace* existing_namespace =
      Context()->GetStorageNamespace(namespace_id);
  if (!existing_namespace) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageCloneNamespaceId::ID, process_handle_);
    delete reply_msg;
    return;
  }
  StorageNamespace* new_namespace = existing_namespace->Copy();
  ViewHostMsg_DOMStorageCloneNamespaceId::WriteReplyParams(reply_msg,
                                                           new_namespace->id());
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnStorageAreaId(int64 namespace_id,
                                               const string16& origin,
                                               IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnStorageAreaId,
        namespace_id, origin, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageNamespace* storage_namespace =
      Context()->GetStorageNamespace(namespace_id);
  if (!storage_namespace) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageStorageAreaId::ID, process_handle_);
    delete reply_msg;
    return;
  }
  StorageArea* storage_area = storage_namespace->GetStorageArea(origin);
  ViewHostMsg_DOMStorageCloneNamespaceId::WriteReplyParams(reply_msg,
                                                           storage_area->id());
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnLength(int64 storage_area_id,
                                        IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnLength, storage_area_id, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageLength::ID, process_handle_);
    delete reply_msg;
    return;
  }
  unsigned length = storage_area->Length();
  ViewHostMsg_DOMStorageLength::WriteReplyParams(reply_msg, length);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnKey(int64 storage_area_id, unsigned index,
                                     IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnKey, storage_area_id, index, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageKey::ID, process_handle_);
    delete reply_msg;
    return;
  }
  const NullableString16& key = storage_area->Key(index);
  ViewHostMsg_DOMStorageKey::WriteReplyParams(reply_msg, key);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnGetItem(int64 storage_area_id,
                                         const string16& key,
                                         IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnGetItem,
        storage_area_id, key, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageGetItem::ID, process_handle_);
    delete reply_msg;
    return;
  }
  const NullableString16& value = storage_area->GetItem(key);
  ViewHostMsg_DOMStorageGetItem::WriteReplyParams(reply_msg, value);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnSetItem(int64 storage_area_id,
    const string16& key, const string16& value, IPC::Message* reply_msg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnSetItem, storage_area_id, key, value,
        reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  bool quota_exception = false;
  StorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageSetItem::ID, process_handle_);
    return;
  }

  ScopedStorageEventContext scope(this);
  storage_area->SetItem(key, value, &quota_exception);
  ViewHostMsg_DOMStorageSetItem::WriteReplyParams(reply_msg, quota_exception);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnRemoveItem(
    int64 storage_area_id, const string16& key) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnRemoveItem, storage_area_id, key));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageRemoveItem::ID, process_handle_);
    return;
  }

  ScopedStorageEventContext scope(this);
  storage_area->RemoveItem(key);
}

void DOMStorageDispatcherHost::OnClear(int64 storage_area_id) {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    PostTaskToWebKitThread(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnClear, storage_area_id));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageClear::ID, process_handle_);
    return;
  }

  ScopedStorageEventContext scope(this);
  storage_area->Clear();
}

void DOMStorageDispatcherHost::OnStorageEvent(
    const ViewMsg_DOMStorageEvent_Params& params) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  const DOMStorageContext::DispatcherHostSet* set =
      Context()->GetDispatcherHostSet();
  DOMStorageContext::DispatcherHostSet::const_iterator cur = set->begin();
  while (cur != set->end()) {
    (*cur)->Send(new ViewMsg_DOMStorageEvent(params));
    ++cur;
  }
}

void DOMStorageDispatcherHost::PostTaskToWebKitThread(
    const tracked_objects::Location& from_here, Task* task) {
  webkit_thread_->EnsureInitialized();
  ChromeThread::PostTask(ChromeThread::WEBKIT, FROM_HERE, task);
}
