// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_

#include <string>
#include <vector>

#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

// Sends appcache related messages to a child process.
class AppCacheFrontendProxy : public blink::mojom::AppCacheFrontend {
 public:
  explicit AppCacheFrontendProxy(int process_id);
  ~AppCacheFrontendProxy() override;

  // AppCacheFrontend methods
  void CacheSelected(int32_t host_id,
                     blink::mojom::AppCacheInfoPtr info) override;
  void EventRaised(const std::vector<int32_t>& host_ids,
                   blink::mojom::AppCacheEventID event_id) override;
  void ProgressEventRaised(const std::vector<int32_t>& host_ids,
                           const GURL& url,
                           int32_t num_total,
                           int32_t num_complete) override;
  void ErrorEventRaised(const std::vector<int32_t>& host_ids,
                        blink::mojom::AppCacheErrorDetailsPtr details) override;
  void LogMessage(int32_t host_id,
                  blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message) override;
  void SetSubresourceFactory(
      int32_t host_id,
      network::mojom::URLLoaderFactoryPtr url_loader_factory) override;

 private:
  blink::mojom::AppCacheFrontend* GetAppCacheFrontend();

  const int process_id_;
  blink::mojom::AppCacheFrontendPtr app_cache_renderer_ptr_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_PROXY_H_
