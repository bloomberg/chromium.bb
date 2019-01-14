// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_APPCACHE_INTERFACES_H_
#define CONTENT_COMMON_APPCACHE_INTERFACES_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/public/common/appcache_info.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace net {
class URLRequest;
}

namespace content {

// Temporarily renumber them in wierd way, to help remove LOG_TIP from WebKit
enum AppCacheLogLevel {
  APPCACHE_LOG_VERBOSE,
  APPCACHE_LOG_INFO,
  APPCACHE_LOG_WARNING,
  APPCACHE_LOG_ERROR
};

enum AppCacheNamespaceType {
  APPCACHE_FALLBACK_NAMESPACE,
  APPCACHE_INTERCEPT_NAMESPACE,
  APPCACHE_NETWORK_NAMESPACE
};

struct CONTENT_EXPORT AppCacheNamespace {
  AppCacheNamespace();  // Type is APPCACHE_FALLBACK_NAMESPACE by default.
  AppCacheNamespace(AppCacheNamespaceType type, const GURL& url,
      const GURL& target, bool is_pattern);
  ~AppCacheNamespace();

  bool IsMatch(const GURL& url) const;

  AppCacheNamespaceType type;
  GURL namespace_url;
  GURL target_url;
  bool is_pattern;
};

// Interface used by backend (browser-process) to talk to frontend (renderer).
class CONTENT_EXPORT AppCacheFrontend {
 public:
  virtual void OnCacheSelected(int host_id,
                               const blink::mojom::AppCacheInfo& info) = 0;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               blink::mojom::AppCacheStatus status) = 0;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             blink::mojom::AppCacheEventID event_id) = 0;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) = 0;
  virtual void OnErrorEventRaised(
      const std::vector<int>& host_ids,
      const blink::mojom::AppCacheErrorDetails& details) = 0;
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) = 0;
  virtual void OnLogMessage(int host_id, AppCacheLogLevel log_level,
                            const std::string& message) = 0;
  // In the network service world, we pass the URLLoaderFactory instance to be
  // used to issue subresource requeste in the |loader_factory_pipe_handle|
  // parameter.
  virtual void OnSetSubresourceFactory(
      int host_id,
      network::mojom::URLLoaderFactoryPtr url_loader_factory) = 0;

  virtual ~AppCacheFrontend() {}
};

// Useful string constants.
CONTENT_EXPORT extern const char kHttpGETMethod[];
CONTENT_EXPORT extern const char kHttpHEADMethod[];

CONTENT_EXPORT bool IsSchemeSupportedForAppCache(const GURL& url);
CONTENT_EXPORT bool IsMethodSupportedForAppCache(
    const std::string& method);

}  // namespace

#endif  // CONTENT_COMMON_APPCACHE_INTERFACES_H_
