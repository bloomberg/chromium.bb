// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_entry.h"

namespace content {

AppCacheURLLoaderJob::~AppCacheURLLoaderJob() {}

void AppCacheURLLoaderJob::Kill() {}

bool AppCacheURLLoaderJob::IsStarted() const {
  return false;
}

bool AppCacheURLLoaderJob::IsWaiting() const {
  return false;
}

bool AppCacheURLLoaderJob::IsDeliveringAppCacheResponse() const {
  return false;
}

bool AppCacheURLLoaderJob::IsDeliveringNetworkResponse() const {
  return false;
}

bool AppCacheURLLoaderJob::IsDeliveringErrorResponse() const {
  return false;
}

bool AppCacheURLLoaderJob::IsCacheEntryNotFound() const {
  return false;
}

void AppCacheURLLoaderJob::DeliverAppCachedResponse(const GURL& manifest_url,
                                                    int64_t cache_id,
                                                    const AppCacheEntry& entry,
                                                    bool is_fallback) {}

void AppCacheURLLoaderJob::DeliverNetworkResponse() {}

void AppCacheURLLoaderJob::DeliverErrorResponse() {}

const GURL& AppCacheURLLoaderJob::GetURL() const {
  return url_;
}

AppCacheURLLoaderJob::AppCacheURLLoaderJob() {}

}  // namespace content
