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

DOMStorageDispatcherHost::DOMStorageDispatcherHost(
    IPC::Message::Sender* message_sender, WebKitContext* webkit_context,
    WebKitThread* webkit_thread)
    : webkit_context_(webkit_context),
      webkit_thread_(webkit_thread),
      message_sender_(message_sender),
      process_handle_(0),
      ever_used_(false),
      shutdown_(false) {
  DCHECK(webkit_context_.get());
  DCHECK(webkit_thread_);
  DCHECK(message_sender_);
}

DOMStorageDispatcherHost::~DOMStorageDispatcherHost() {
  DCHECK(shutdown_);
}

void DOMStorageDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(!process_handle_);
  process_handle_ = process_handle;
  DCHECK(process_handle_);
}

void DOMStorageDispatcherHost::Shutdown() {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    message_sender_ = NULL;
    if (!ever_used_) {
      // No need to (possibly) spin up the WebKit thread for a no-op.
      shutdown_ = true;
      return;
    }

    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::Shutdown));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(ever_used_);
  DCHECK(!message_sender_);
  DCHECK(!shutdown_);
  shutdown_ = true;

  // TODO(jorlow): If we have any locks or are tracking resources that need to
  //               be released on the WebKit thread, do it here.
}

bool DOMStorageDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool *msg_is_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!shutdown_);
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageSetItem, OnSetItem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageRemoveItem, OnRemoveItem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageClear, OnClear)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    ever_used_ = true;
  return handled;
}

void DOMStorageDispatcherHost::Send(IPC::Message* message) {
  DCHECK(!shutdown_);
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
  webkit_thread_->PostIOThreadTask(FROM_HERE, NewRunnableMethod(this,
      &DOMStorageDispatcherHost::Send, message));
}

void DOMStorageDispatcherHost::OnNamespaceId(DOMStorageType storage_type,
                                             IPC::Message* reply_msg) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
                                         const string16& key,
                                         const string16& value) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnSetItem, storage_area_id, key, value));
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
  storage_area->SetItem(key, value, &quota_exception);
  DCHECK(!quota_exception);  // This is tracked by the renderer.
}

void DOMStorageDispatcherHost::OnRemoveItem(int64 storage_area_id,
                                            const string16& key) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  storage_area->RemoveItem(key);
}

void DOMStorageDispatcherHost::OnClear(int64 storage_area_id) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
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
  storage_area->Clear();
}
