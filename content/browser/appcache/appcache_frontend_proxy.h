// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_

#include <string>
#include <vector>

#include "content/common/appcache_interfaces.h"
#include "ipc/ipc_sender.h"

namespace content {

// Sends appcache related messages to a child process.
class AppCacheFrontendProxy : public AppCacheFrontend {
 public:
  explicit AppCacheFrontendProxy(IPC::Sender* sender);

  // AppCacheFrontend methods
  virtual void OnCacheSelected(int host_id,
                               const AppCacheInfo& info) override;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               AppCacheStatus status) override;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             AppCacheEventID event_id) override;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) override;
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const AppCacheErrorDetails& details)
      override;
  virtual void OnLogMessage(int host_id, AppCacheLogLevel log_level,
                            const std::string& message) override;
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) override;

 private:
  IPC::Sender* sender_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
