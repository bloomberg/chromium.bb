// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_dispatcher_host.h"

#include "base/callback.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/common/appcache_messages.h"

AppCacheDispatcherHost::AppCacheDispatcherHost(
    net::URLRequestContext* request_context,
    int process_id)
    : ALLOW_THIS_IN_INITIALIZER_LIST(frontend_proxy_(this)),
      request_context_(request_context),
      process_id_(process_id) {
  DCHECK(request_context_.get());
}

AppCacheDispatcherHost::AppCacheDispatcherHost(
    URLRequestContextGetter* request_context_getter,
    int process_id)
    : ALLOW_THIS_IN_INITIALIZER_LIST(frontend_proxy_(this)),
      request_context_getter_(request_context_getter),
      process_id_(process_id) {
  DCHECK(request_context_getter_.get());
}

AppCacheDispatcherHost::~AppCacheDispatcherHost() {}

void AppCacheDispatcherHost::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  DCHECK(request_context_.get() || request_context_getter_.get());

  // Get the AppCacheService (it can only be accessed from IO thread).
  net::URLRequestContext* context = request_context_.get();
  if (!context)
    context = request_context_getter_->GetURLRequestContext();
  appcache_service_ =
      static_cast<ChromeURLRequestContext*>(context)->appcache_service();
  request_context_ = NULL;
  request_context_getter_ = NULL;

  if (appcache_service_.get()) {
    backend_impl_.Initialize(
        appcache_service_.get(), &frontend_proxy_, process_id_);
    get_status_callback_.reset(
        NewCallback(this, &AppCacheDispatcherHost::GetStatusCallback));
    start_update_callback_.reset(
        NewCallback(this, &AppCacheDispatcherHost::StartUpdateCallback));
    swap_cache_callback_.reset(
        NewCallback(this, &AppCacheDispatcherHost::SwapCacheCallback));
  }
}

bool AppCacheDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                               bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AppCacheDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_RegisterHost, OnRegisterHost)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_UnregisterHost, OnUnregisterHost)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_GetResourceList, OnGetResourceList)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SelectCache, OnSelectCache)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SelectCacheForWorker,
                        OnSelectCacheForWorker)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SelectCacheForSharedWorker,
                        OnSelectCacheForSharedWorker)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_MarkAsForeignEntry,
                        OnMarkAsForeignEntry)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheHostMsg_GetStatus, OnGetStatus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheHostMsg_StartUpdate, OnStartUpdate)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheHostMsg_SwapCache, OnSwapCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void AppCacheDispatcherHost::BadMessageReceived() {
  UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_ACDH"));
  BrowserMessageFilter::BadMessageReceived();
}

void AppCacheDispatcherHost::OnRegisterHost(int host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.RegisterHost(host_id)) {
      BadMessageReceived();
    }
  }
}

void AppCacheDispatcherHost::OnUnregisterHost(int host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.UnregisterHost(host_id)) {
      BadMessageReceived();
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
      BadMessageReceived();
    }
  } else {
    frontend_proxy_.OnCacheSelected(host_id, appcache::AppCacheInfo());
  }
}

void AppCacheDispatcherHost::OnSelectCacheForWorker(
    int host_id, int parent_process_id, int parent_host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SelectCacheForWorker(
            host_id, parent_process_id, parent_host_id)) {
      BadMessageReceived();
    }
  } else {
    frontend_proxy_.OnCacheSelected(host_id, appcache::AppCacheInfo());
  }
}

void AppCacheDispatcherHost::OnSelectCacheForSharedWorker(
    int host_id, int64 appcache_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SelectCacheForSharedWorker(host_id, appcache_id))
      BadMessageReceived();
  } else {
    frontend_proxy_.OnCacheSelected(host_id, appcache::AppCacheInfo());
  }
}

void AppCacheDispatcherHost::OnMarkAsForeignEntry(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  if (appcache_service_.get()) {
    if (!backend_impl_.MarkAsForeignEntry(host_id, document_url,
                                          cache_document_was_loaded_from)) {
      BadMessageReceived();
    }
  }
}

void AppCacheDispatcherHost::OnGetResourceList(
    int host_id, std::vector<appcache::AppCacheResourceInfo>* params) {
  if (appcache_service_.get())
    backend_impl_.GetResourceList(host_id, params);
}

void AppCacheDispatcherHost::OnGetStatus(int host_id,
                                         IPC::Message* reply_msg) {
  if (pending_reply_msg_.get()) {
    BadMessageReceived();
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.GetStatusWithCallback(
            host_id, get_status_callback_.get(), reply_msg)) {
      BadMessageReceived();
    }
    return;
  }

  GetStatusCallback(appcache::UNCACHED, reply_msg);
}

void AppCacheDispatcherHost::OnStartUpdate(int host_id,
                                           IPC::Message* reply_msg) {
  if (pending_reply_msg_.get()) {
    BadMessageReceived();
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.StartUpdateWithCallback(
            host_id, start_update_callback_.get(), reply_msg)) {
      BadMessageReceived();
    }
    return;
  }

  StartUpdateCallback(false, reply_msg);
}

void AppCacheDispatcherHost::OnSwapCache(int host_id,
                                         IPC::Message* reply_msg) {
  if (pending_reply_msg_.get()) {
    BadMessageReceived();
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.SwapCacheWithCallback(
            host_id, swap_cache_callback_.get(), reply_msg)) {
      BadMessageReceived();
    }
    return;
  }

  SwapCacheCallback(false, reply_msg);
}

void AppCacheDispatcherHost::GetStatusCallback(
    appcache::Status status, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK_EQ(pending_reply_msg_.get(), reply_msg);
  AppCacheHostMsg_GetStatus::WriteReplyParams(reply_msg, status);
  Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::StartUpdateCallback(bool result, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK_EQ(pending_reply_msg_.get(), reply_msg);
  AppCacheHostMsg_StartUpdate::WriteReplyParams(reply_msg, result);
  Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::SwapCacheCallback(bool result, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK_EQ(pending_reply_msg_.get(), reply_msg);
  AppCacheHostMsg_SwapCache::WriteReplyParams(reply_msg, result);
  Send(pending_reply_msg_.release());
}
