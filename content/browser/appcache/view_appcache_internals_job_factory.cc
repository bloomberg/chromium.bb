// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/view_appcache_internals_job_factory.h"

#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/view_appcache_internals_job.h"

// static.
bool ViewAppCacheInternalsJobFactory::IsSupportedURL(const GURL& url) {
  return StartsWithASCII(url.spec(),
                         chrome::kAppCacheViewInternalsURL,
                         true /*case_sensitive*/);
}

// static.
net::URLRequestJob* ViewAppCacheInternalsJobFactory::CreateJobForRequest(
    net::URLRequest* request, ChromeAppCacheService* appcache_service) {
  return new appcache::ViewAppCacheInternalsJob(
      request, appcache_service);
}
