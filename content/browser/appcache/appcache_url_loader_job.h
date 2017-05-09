// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_

#include "base/logging.h"
#include "base/strings/string16.h"
#include "content/browser/appcache/appcache_job.h"
#include "content/common/content_export.h"

namespace content {

class AppCacheHost;
class AppCacheRequest;
class AppCacheStorage;

// AppCacheJob wrapper for a mojom::URLLoader implementation which returns
// responses stored in the AppCache.
class CONTENT_EXPORT AppCacheURLLoaderJob : public AppCacheJob {
 public:
  ~AppCacheURLLoaderJob() override;

  // AppCacheJob overrides.
  void Kill() override;
  bool IsStarted() const override;
  bool IsWaiting() const override;
  bool IsDeliveringAppCacheResponse() const override;
  bool IsDeliveringNetworkResponse() const override;
  bool IsDeliveringErrorResponse() const override;
  bool IsCacheEntryNotFound() const override;
  void DeliverAppCachedResponse(const GURL& manifest_url,
                                int64_t cache_id,
                                const AppCacheEntry& entry,
                                bool is_fallback) override;
  void DeliverNetworkResponse() override;
  void DeliverErrorResponse() override;
  const GURL& GetURL() const override;

 protected:
  // AppCacheJob::Create() creates this instance.
  friend class AppCacheJob;

  AppCacheURLLoaderJob();

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheURLLoaderJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_
