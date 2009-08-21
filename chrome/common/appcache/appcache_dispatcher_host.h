// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPCACHE_APPCACHE_DISPATCHER_HOST_H_
#define CHROME_COMMON_APPCACHE_APPCACHE_DISPATCHER_HOST_H_

#include <vector>
#include "chrome/common/appcache/appcache_frontend_proxy.h"
#include "ipc/ipc_message.h"
#include "webkit/appcache/appcache_backend_impl.h"

// Handles appcache related messages sent to the main browser process from
// its child processes. There is a distinct host for each child process.
// Messages are handled on the IO thread. The ResourceMessageFilter creates
// an instance and delegates calls to it.
class AppCacheDispatcherHost {
 public:
  void Initialize(IPC::Message::Sender* sender);
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_is_ok);

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

  AppCacheFrontendProxy frontend_proxy_;
  appcache::AppCacheBackendImpl backend_impl_;
};

#endif  // CHROME_COMMON_APPCACHE_APPCACHE_DISPATCHER_HOST_H_
