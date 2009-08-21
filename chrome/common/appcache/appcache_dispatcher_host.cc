// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/appcache/appcache_dispatcher_host.h"

#include "chrome/common/render_messages.h"

void AppCacheDispatcherHost::Initialize(IPC::Message::Sender* sender) {
  DCHECK(sender);
  frontend_proxy_.set_sender(sender);
  backend_impl_.Initialize(NULL, &frontend_proxy_);
}

bool AppCacheDispatcherHost::OnMessageReceived(const IPC::Message& msg,
                                               bool *msg_ok) {
  DCHECK(frontend_proxy_.sender());
  *msg_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AppCacheDispatcherHost, msg, *msg_ok)
    IPC_MESSAGE_HANDLER(AppCacheMsg_RegisterHost, OnRegisterHost);
    IPC_MESSAGE_HANDLER(AppCacheMsg_UnregisterHost, OnUnregisterHost);
    IPC_MESSAGE_HANDLER(AppCacheMsg_SelectCache, OnSelectCache);
    IPC_MESSAGE_HANDLER(AppCacheMsg_MarkAsForeignEntry, OnMarkAsForeignEntry);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheMsg_GetStatus, OnGetStatus);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheMsg_StartUpdate, OnStartUpdate);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheMsg_SwapCache, OnSwapCache);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void AppCacheDispatcherHost::OnRegisterHost(int host_id) {
  backend_impl_.RegisterHost(host_id);
}

void AppCacheDispatcherHost::OnUnregisterHost(int host_id) {
  backend_impl_.UnregisterHost(host_id);
}

void AppCacheDispatcherHost::OnSelectCache(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from,
    const GURL& opt_manifest_url) {
  backend_impl_.SelectCache(host_id, document_url,
                            cache_document_was_loaded_from,
                            opt_manifest_url);
}

void AppCacheDispatcherHost::OnMarkAsForeignEntry(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  backend_impl_.MarkAsForeignEntry(host_id, document_url,
                                   cache_document_was_loaded_from);
}

void AppCacheDispatcherHost::OnGetStatus(int host_id,
                                         IPC::Message* reply_msg) {
  // TODO(michaeln): Handle the case where cache selection is not yet complete.
  appcache::Status status = backend_impl_.GetStatus(host_id);
  AppCacheMsg_GetStatus::WriteReplyParams(reply_msg, status);
  frontend_proxy_.sender()->Send(reply_msg);
}

void AppCacheDispatcherHost::OnStartUpdate(int host_id,
                                           IPC::Message* reply_msg) {
  // TODO(michaeln): Handle the case where cache selection is not yet complete.
  bool success = backend_impl_.StartUpdate(host_id);
  AppCacheMsg_StartUpdate::WriteReplyParams(reply_msg, success);
  frontend_proxy_.sender()->Send(reply_msg);
}

void AppCacheDispatcherHost::OnSwapCache(int host_id,
                                         IPC::Message* reply_msg) {
  // TODO(michaeln): Handle the case where cache selection is not yet complete.
  bool success = backend_impl_.SwapCache(host_id);
  AppCacheMsg_SwapCache::WriteReplyParams(reply_msg, success);
  frontend_proxy_.sender()->Send(reply_msg);
}
