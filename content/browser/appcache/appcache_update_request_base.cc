// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_request_base.h"
#include "content/browser/appcache/appcache_update_url_request.h"
#include "net/url_request/url_request_context.h"

namespace content {

AppCacheUpdateJob::UpdateRequestBase::~UpdateRequestBase() {}

// static
std::unique_ptr<AppCacheUpdateJob::UpdateRequestBase>
AppCacheUpdateJob::UpdateRequestBase::Create(
    net::URLRequestContext* request_context,
    const GURL& url,
    URLFetcher* fetcher) {
  return std::unique_ptr<UpdateRequestBase>(
      new UpdateURLRequest(request_context, url, fetcher));
}

AppCacheUpdateJob::UpdateRequestBase::UpdateRequestBase() {}

}  // namespace content
