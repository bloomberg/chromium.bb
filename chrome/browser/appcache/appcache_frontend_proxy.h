// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
#define CHROME_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_

#include <vector>

#include "ipc/ipc_message.h"
#include "webkit/appcache/appcache_interfaces.h"

// Sends appcache related messages to a child process.
class AppCacheFrontendProxy : public appcache::AppCacheFrontend {
 public:
  AppCacheFrontendProxy() : sender_(NULL) {}
  void set_sender(IPC::Message::Sender* sender) { sender_ = sender; }
  IPC::Message::Sender* sender() const { return sender_; }

  // AppCacheFrontend methods
  virtual void OnCacheSelected(int host_id, int64 cache_id ,
                               appcache::Status);
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               appcache::Status status);
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             appcache::EventID event_id);
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete);
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message);
  virtual void OnLogMessage(int host_id, appcache::LogLevel log_level,
                            const std::string& message);
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url);

 private:
  IPC::Message::Sender* sender_;
};

#endif  // CHROME_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
