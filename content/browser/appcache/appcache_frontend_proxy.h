// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
#pragma once

#include <string>
#include <vector>

#include "ipc/ipc_message.h"
#include "webkit/appcache/appcache_interfaces.h"

// Sends appcache related messages to a child process.
class AppCacheFrontendProxy : public appcache::AppCacheFrontend {
 public:
  explicit AppCacheFrontendProxy(IPC::Message::Sender* sender);

  // AppCacheFrontend methods
  virtual void OnCacheSelected(int host_id,
                               const appcache::AppCacheInfo& info) OVERRIDE;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               appcache::Status status) OVERRIDE;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             appcache::EventID event_id) OVERRIDE;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) OVERRIDE;
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message) OVERRIDE;
  virtual void OnLogMessage(int host_id, appcache::LogLevel log_level,
                            const std::string& message) OVERRIDE;
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) OVERRIDE;

 private:
  IPC::Message::Sender* sender_;
};

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
