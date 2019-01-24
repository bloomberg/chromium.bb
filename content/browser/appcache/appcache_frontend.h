// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

class GURL;

namespace content {

// Interface used by backend (browser-process) to talk to frontend (renderer).
class CONTENT_EXPORT AppCacheFrontend {
 public:
  virtual ~AppCacheFrontend() = default;

  virtual void OnCacheSelected(int host_id,
                               const blink::mojom::AppCacheInfo& info) = 0;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               blink::mojom::AppCacheStatus status) = 0;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             blink::mojom::AppCacheEventID event_id) = 0;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total,
                                     int num_complete) = 0;
  virtual void OnErrorEventRaised(
      const std::vector<int>& host_ids,
      const blink::mojom::AppCacheErrorDetails& details) = 0;
  virtual void OnContentBlocked(int host_id, const GURL& manifest_url) = 0;
  virtual void OnLogMessage(int host_id,
                            AppCacheLogLevel log_level,
                            const std::string& message) = 0;
  // In the network service world, we pass the URLLoaderFactory instance to be
  // used to issue subresource requeste in the |loader_factory_pipe_handle|
  // parameter.
  virtual void OnSetSubresourceFactory(
      int host_id,
      network::mojom::URLLoaderFactoryPtr url_loader_factory) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_FRONTEND_H_