// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPCACHE_APPCACHE_DISPATCHER_HOST_H_
#define CHROME_COMMON_APPCACHE_APPCACHE_DISPATCHER_HOST_H_

#include <vector>

#include "base/process.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/appcache/appcache_frontend_proxy.h"
#include "ipc/ipc_message.h"
#include "webkit/appcache/appcache_backend_impl.h"

class ChromeAppCacheService;
class URLRequestContextGetter;

// Handles appcache related messages sent to the main browser process from
// its child processes. There is a distinct host for each child process.
// Messages are handled on the IO thread. The ResourceMessageFilter creates
// an instance and delegates calls to it.
class AppCacheDispatcherHost {
 public:
  explicit AppCacheDispatcherHost(
      URLRequestContextGetter* request_context_getter);

  void Initialize(IPC::Message::Sender* sender, int process_id,
                  base::ProcessHandle process_handle);
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_is_ok);

  int process_id() const { return backend_impl_.process_id(); }

  // Note: needed to satisfy ipc message dispatching macros.
  bool Send(IPC::Message* msg) {
    return frontend_proxy_.sender()->Send(msg);
  }

 private:
  // Ipc message handlers
  void OnRegisterHost(int host_id);
  void OnUnregisterHost(int host_id);
  void OnSelectCache(int host_id, const GURL& document_url,
                     int64 cache_document_was_loaded_from,
                     const GURL& opt_manifest_url);
  void OnMarkAsForeignEntry(int host_id, const GURL& document_url,
                            int64 cache_document_was_loaded_from);
  void OnGetStatus(int host_id, IPC::Message* reply_msg);
  void OnStartUpdate(int host_id, IPC::Message* reply_msg);
  void OnSwapCache(int host_id, IPC::Message* reply_msg);

  void GetStatusCallback(appcache::Status status, void* param);
  void StartUpdateCallback(bool result, void* param);
  void SwapCacheCallback(bool result, void* param);

  void ReceivedBadMessage(uint32 msg_type);

  AppCacheFrontendProxy frontend_proxy_;
  appcache::AppCacheBackendImpl backend_impl_;

  // Temporary until Initialize() can be called from the IO thread,
  // which will extract the AppCacheService from the URLRequestContext.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  // This is only valid once Initialize() has been called.
  scoped_refptr<ChromeAppCacheService> appcache_service_;

  scoped_ptr<appcache::GetStatusCallback> get_status_callback_;
  scoped_ptr<appcache::StartUpdateCallback> start_update_callback_;
  scoped_ptr<appcache::SwapCacheCallback> swap_cache_callback_;
  base::ProcessHandle process_handle_;
  scoped_ptr<IPC::Message> pending_reply_msg_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheDispatcherHost);
};

#endif  // CHROME_COMMON_APPCACHE_APPCACHE_DISPATCHER_HOST_H_
