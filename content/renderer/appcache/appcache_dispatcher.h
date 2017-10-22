// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_APPCACHE_APPCACHE_DISPATCHER_H_
#define CONTENT_RENDERER_APPCACHE_APPCACHE_DISPATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "content/common/appcache_interfaces.h"
#include "content/renderer/appcache/appcache_backend_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

// Dispatches appcache related messages sent to a child process from the
// main browser process. There is one instance per child process. Messages
// are dispatched on the main child thread. The ChildThread base class
// creates an instance and delegates calls to it.
class AppCacheDispatcher : public IPC::Listener {
 public:
  AppCacheDispatcher(IPC::Sender* sender,
                     AppCacheFrontend* frontend);
  ~AppCacheDispatcher() override;

  AppCacheBackendProxy* backend_proxy() { return &backend_proxy_; }

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Ipc message handlers
  void OnCacheSelected(int host_id, const AppCacheInfo& info);
  void OnStatusChanged(const std::vector<int>& host_ids,
                       AppCacheStatus status);
  void OnEventRaised(const std::vector<int>& host_ids,
                     AppCacheEventID event_id);
  void OnProgressEventRaised(const std::vector<int>& host_ids,
                             const GURL& url, int num_total, int num_complete);
  void OnErrorEventRaised(const std::vector<int>& host_ids,
                          const AppCacheErrorDetails& details);
  void OnLogMessage(int host_id, int log_level, const std::string& message);
  void OnContentBlocked(int host_id, const GURL& manifest_url);
  void OnSetSubresourceFactory(int host_id,
                               mojo::MessagePipeHandle loader_factory_pipe);
  AppCacheBackendProxy backend_proxy_;
  std::unique_ptr<AppCacheFrontend> frontend_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_APPCACHE_APPCACHE_DISPATCHER_H_
