// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/appcache/appcache_dispatcher_host.h"

#include "chrome/browser/renderer_host/browser_render_process_host.h"
// TODO(eroman): uh oh, depending on stuff outside of common/
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/appcache/chrome_appcache_service.h"
#include "chrome/common/render_messages.h"

AppCacheDispatcherHost::AppCacheDispatcherHost(
    URLRequestContextGetter* request_context_getter)
        : request_context_getter_(request_context_getter),
          process_handle_(0) {
  DCHECK(request_context_getter_.get());
}

void AppCacheDispatcherHost::Initialize(IPC::Message::Sender* sender,
    int process_id, base::ProcessHandle process_handle) {
  DCHECK(sender);
  DCHECK(process_handle && !process_handle_);
  DCHECK(request_context_getter_.get());

  process_handle_ = process_handle;

  // Get the AppCacheService (it can only be accessed from IO thread).
  URLRequestContext* context = request_context_getter_->GetURLRequestContext();
  appcache_service_ =
      static_cast<ChromeURLRequestContext*>(context)->appcache_service();
  request_context_getter_ = NULL;

  frontend_proxy_.set_sender(sender);
  if (appcache_service_.get()) {
    backend_impl_.Initialize(
        appcache_service_.get(), &frontend_proxy_, process_id);
    get_status_callback_.reset(
        NewCallback(this, &AppCacheDispatcherHost::GetStatusCallback));
    start_update_callback_.reset(
        NewCallback(this, &AppCacheDispatcherHost::StartUpdateCallback));
    swap_cache_callback_.reset(
        NewCallback(this, &AppCacheDispatcherHost::SwapCacheCallback));
  }
}

bool AppCacheDispatcherHost::OnMessageReceived(const IPC::Message& msg,
                                               bool *msg_ok) {
  DCHECK(process_handle_);
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
  if (appcache_service_.get()) {
    if (!backend_impl_.RegisterHost(host_id)) {
      ReceivedBadMessage(AppCacheMsg_RegisterHost::ID);
    }
  }
}

void AppCacheDispatcherHost::OnUnregisterHost(int host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.UnregisterHost(host_id)) {
      ReceivedBadMessage(AppCacheMsg_UnregisterHost::ID);
    }
  }
}

void AppCacheDispatcherHost::OnSelectCache(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from,
    const GURL& opt_manifest_url) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SelectCache(host_id, document_url,
                                   cache_document_was_loaded_from,
                                   opt_manifest_url)) {
      ReceivedBadMessage(AppCacheMsg_SelectCache::ID);
    }
  } else {
    frontend_proxy_.OnCacheSelected(
        host_id, appcache::kNoCacheId, appcache::UNCACHED);
  }
}

void AppCacheDispatcherHost::OnMarkAsForeignEntry(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  if (appcache_service_.get()) {
    if (!backend_impl_.MarkAsForeignEntry(host_id, document_url,
                                          cache_document_was_loaded_from)) {
      ReceivedBadMessage(AppCacheMsg_MarkAsForeignEntry::ID);
    }
  }
}

void AppCacheDispatcherHost::OnGetStatus(int host_id,
                                         IPC::Message* reply_msg) {
  if (pending_reply_msg_.get()) {
    ReceivedBadMessage(AppCacheMsg_GetStatus::ID);
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.GetStatusWithCallback(
            host_id, get_status_callback_.get(), reply_msg)) {
      ReceivedBadMessage(AppCacheMsg_GetStatus::ID);
    }
    return;
  }

  GetStatusCallback(appcache::UNCACHED, reply_msg);
}

void AppCacheDispatcherHost::OnStartUpdate(int host_id,
                                           IPC::Message* reply_msg) {
  if (pending_reply_msg_.get()) {
    ReceivedBadMessage(AppCacheMsg_StartUpdate::ID);
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.StartUpdateWithCallback(
            host_id, start_update_callback_.get(), reply_msg)) {
      ReceivedBadMessage(AppCacheMsg_StartUpdate::ID);
    }
    return;
  }

  StartUpdateCallback(false, reply_msg);
}

void AppCacheDispatcherHost::OnSwapCache(int host_id,
                                         IPC::Message* reply_msg) {
  if (pending_reply_msg_.get()) {
    ReceivedBadMessage(AppCacheMsg_SwapCache::ID);
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.SwapCacheWithCallback(
            host_id, swap_cache_callback_.get(), reply_msg)) {
      ReceivedBadMessage(AppCacheMsg_SwapCache::ID);
    }
    return;
  }

  SwapCacheCallback(false, reply_msg);
}

void AppCacheDispatcherHost::GetStatusCallback(
    appcache::Status status, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK(reply_msg == pending_reply_msg_.get());
  AppCacheMsg_GetStatus::WriteReplyParams(reply_msg, status);
  frontend_proxy_.sender()->Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::StartUpdateCallback(bool result, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK(reply_msg == pending_reply_msg_.get());
  AppCacheMsg_StartUpdate::WriteReplyParams(reply_msg, result);
  frontend_proxy_.sender()->Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::SwapCacheCallback(bool result, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK(reply_msg == pending_reply_msg_.get());
  AppCacheMsg_SwapCache::WriteReplyParams(reply_msg, result);
  frontend_proxy_.sender()->Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::ReceivedBadMessage(uint32 msg_type) {
  // TODO(michaeln): Consider gathering UMA stats
  // http://code.google.com/p/chromium/issues/detail?id=24634
  BrowserRenderProcessHost::BadMessageTerminateProcess(
      msg_type, process_handle_);
}
