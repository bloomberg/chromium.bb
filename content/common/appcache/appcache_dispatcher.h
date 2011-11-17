// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_APPCACHE_APPCACHE_DISPATCHER_H_
#define CONTENT_COMMON_APPCACHE_APPCACHE_DISPATCHER_H_
#pragma once

#include <string>
#include <vector>
#include "content/common/appcache/appcache_backend_proxy.h"
#include "ipc/ipc_channel.h"
#include "webkit/appcache/appcache_frontend_impl.h"

// Dispatches appcache related messages sent to a child process from the
// main browser process. There is one instance per child process. Messages
// are dispatched on the main child thread. The ChildThread base class
// creates an instance and delegates calls to it.
class AppCacheDispatcher : public IPC::Channel::Listener {
 public:
  explicit AppCacheDispatcher(IPC::Message::Sender* sender)
      : backend_proxy_(sender) {}

  AppCacheBackendProxy* backend_proxy() { return &backend_proxy_; }

  // IPC::Channel::Listener implementation
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // Ipc message handlers
  void OnCacheSelected(int host_id, const appcache::AppCacheInfo& info);
  void OnStatusChanged(const std::vector<int>& host_ids,
                       appcache::Status status);
  void OnEventRaised(const std::vector<int>& host_ids,
                     appcache::EventID event_id);
  void OnProgressEventRaised(const std::vector<int>& host_ids,
                             const GURL& url, int num_total, int num_complete);
  void OnErrorEventRaised(const std::vector<int>& host_ids,
                          const std::string& message);
  void OnLogMessage(int host_id, int log_level, const std::string& message);
  void OnContentBlocked(int host_id,
                        const GURL& manifest_url);

  AppCacheBackendProxy backend_proxy_;
  appcache::AppCacheFrontendImpl frontend_impl_;
};

#endif  // CONTENT_COMMON_APPCACHE_APPCACHE_DISPATCHER_H_
