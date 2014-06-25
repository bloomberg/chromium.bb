// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
#define CONTENT_CHILD_APPCACHE_APPCACHE_FRONTEND_IMPL_H_

#include "content/common/appcache_interfaces.h"

namespace content {

class AppCacheFrontendImpl : public AppCacheFrontend {
 public:
  virtual void OnCacheSelected(int host_id,
                               const AppCacheInfo& info) OVERRIDE;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               AppCacheStatus status) OVERRIDE;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             AppCacheEventID event_id) OVERRIDE;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total,
                                     int num_complete) OVERRIDE;
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const AppCacheErrorDetails& details)
      OVERRIDE;
  virtual void OnLogMessage(int host_id,
                            AppCacheLogLevel log_level,
                            const std::string& message) OVERRIDE;
  virtual void OnContentBlocked(int host_id, const GURL& manifest_url) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_CHILD_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
