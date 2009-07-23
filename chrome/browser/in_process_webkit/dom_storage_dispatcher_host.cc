// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"

#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/in_process_webkit/webkit_thread.h"
#include "chrome/common/render_messages.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebStorageNamespace.h"
#include "webkit/api/public/WebString.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;

DOMStorageDispatcherHost::DOMStorageDispatcherHost(
    IPC::Message::Sender* message_sender,
    WebKitContext* webkit_context,
    WebKitThread* webkit_thread)
    : webkit_context_(webkit_context),
      webkit_thread_(webkit_thread),
      message_sender_(message_sender),
      last_storage_area_id_(0),
      last_storage_namespace_id_(0),
      ever_used_(false),
      shutdown_(false) {
  DCHECK(webkit_context_.get());
  DCHECK(webkit_thread_);
  DCHECK(message_sender_);
}

DOMStorageDispatcherHost::~DOMStorageDispatcherHost() {
  DCHECK(!ever_used_ || ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(shutdown_);
}

void DOMStorageDispatcherHost::Shutdown() {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    message_sender_ = NULL;
    if (!ever_used_) {
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

  STLDeleteContainerPairSecondPointers(storage_area_map_.begin(),
                                       storage_area_map_.end());
  STLDeleteContainerPairSecondPointers(storage_namespace_map_.begin(),
                                       storage_namespace_map_.end());
  shutdown_ = true;
}

bool DOMStorageDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                 bool *msg_is_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!shutdown_);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DOMStorageDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageNamespaceId,
                                    OnNamespaceId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageCloneNamespaceId,
                                    OnCloneNamespaceId)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageDerefNamespaceId,
                        OnDerefNamespaceId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageStorageAreaId,
                                    OnStorageAreaId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageLock, OnLock)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageUnlock, OnUnlock)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageLength, OnLength)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageKey, OnKey)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageGetItem, OnGetItem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageSetItem, OnSetItem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMStorageRemoveItem, OnRemoveItem)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageClear, OnClear)
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

void DOMStorageDispatcherHost::OnNamespaceId(bool is_local_storage,
                                             IPC::Message* reply_msg) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnNamespaceId,
        is_local_storage, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebStorageNamespace* new_namespace;
  if (is_local_storage) {
    new_namespace = WebStorageNamespace::createLocalStorageNamespace(
        GetLocalStoragePath());
  } else {
    new_namespace = WebStorageNamespace::createSessionStorageNamespace();
  }
  int64 new_namespace_id = AddStorageNamespace(new_namespace);
  ViewHostMsg_DOMStorageNamespaceId::WriteReplyParams(reply_msg,
                                                      new_namespace_id);
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
  WebStorageNamespace* existing_namespace = GetStorageNamespace(namespace_id);
  CHECK(existing_namespace);  // TODO(jorlow): Do better than this.
  WebStorageNamespace* new_namespace = existing_namespace->copy();
  int64 new_namespace_id = AddStorageNamespace(new_namespace);
  ViewHostMsg_DOMStorageCloneNamespaceId::WriteReplyParams(reply_msg,
                                                           new_namespace_id);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnDerefNamespaceId(int64 namespace_id) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnDerefNamespaceId, namespace_id));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  // TODO(jorlow): We need to track resources so we can free them.
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
  WebStorageNamespace* storage_namespace = GetStorageNamespace(namespace_id);
  CHECK(storage_namespace);  // TODO(jorlow): Do better than this.
  WebStorageArea* storage_area = storage_namespace->createStorageArea(origin);
  int64 storage_area_id = AddStorageArea(storage_area);
  ViewHostMsg_DOMStorageCloneNamespaceId::WriteReplyParams(reply_msg,
                                                           storage_area_id);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnLock(int64 storage_area_id,
                                      IPC::Message* reply_msg) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnLock, storage_area_id, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  bool invalidate_cache;
  size_t bytes_left_in_quota;
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  storage_area->lock(invalidate_cache, bytes_left_in_quota);
  ViewHostMsg_DOMStorageLock::WriteReplyParams(reply_msg, invalidate_cache,
                                               bytes_left_in_quota);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnUnlock(int64 storage_area_id) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnUnlock, storage_area_id));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  storage_area->unlock();
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
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  unsigned length = storage_area->length();
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
  bool key_exception = false;
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  string16 key = storage_area->key(index, key_exception);
  ViewHostMsg_DOMStorageKey::WriteReplyParams(reply_msg, key_exception, key);
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
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  WebKit::WebString value = storage_area->getItem(key);
  ViewHostMsg_DOMStorageGetItem::WriteReplyParams(reply_msg, (string16)value,
                                                  value.isNull());
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
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  storage_area->setItem(key, value, quota_exception);
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
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  storage_area->removeItem(key);
}

void DOMStorageDispatcherHost::OnClear(int64 storage_area_id,
                                       IPC::Message* reply_msg) {
  DCHECK(!shutdown_);
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    MessageLoop* webkit_loop = webkit_thread_->GetMessageLoop();
    webkit_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DOMStorageDispatcherHost::OnClear, storage_area_id, reply_msg));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  // TODO(jorlow): Return the total quota for this domain.
  size_t bytes_left_in_quota = 0;
  WebStorageArea* storage_area = GetStorageArea(storage_area_id);
  CHECK(storage_area);  // TODO(jorlow): Do better than this.
  storage_area->clear();
  ViewHostMsg_DOMStorageClear::WriteReplyParams(reply_msg,
                                                bytes_left_in_quota);
  Send(reply_msg);
}

WebStorageArea* DOMStorageDispatcherHost::GetStorageArea(int64 id) {
  StorageAreaMap::iterator iterator = storage_area_map_.find(id);
  if (iterator == storage_area_map_.end())
    return NULL;
  return iterator->second;
}

WebStorageNamespace* DOMStorageDispatcherHost::GetStorageNamespace(int64 id) {
  StorageNamespaceMap::iterator iterator = storage_namespace_map_.find(id);
  if (iterator == storage_namespace_map_.end())
    return NULL;
  return iterator->second;
}

int64 DOMStorageDispatcherHost::AddStorageArea(
    WebStorageArea* new_storage_area) {
  // Create a new ID and insert it into our map.
  int64 new_storage_area_id = ++last_storage_area_id_;
  DCHECK(!GetStorageArea(new_storage_area_id));
  storage_area_map_[new_storage_area_id] = new_storage_area;
  return new_storage_area_id;
}

int64 DOMStorageDispatcherHost::AddStorageNamespace(
    WebStorageNamespace* new_namespace) {
  // Create a new ID and insert it into our map.
  int64 new_namespace_id = ++last_storage_namespace_id_;
  DCHECK(!GetStorageNamespace(new_namespace_id));
  storage_namespace_map_[new_namespace_id] = new_namespace;
  return new_namespace_id;
}

string16 DOMStorageDispatcherHost::GetLocalStoragePath() {
  // TODO(jorlow): Create a path based on the WebKitContext.
  string16 path;
  return path;
}
