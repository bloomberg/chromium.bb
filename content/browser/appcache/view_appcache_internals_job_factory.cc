// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/view_appcache_internals_job_factory.h"

#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/view_appcache_internals_job.h"

// static.
bool ViewAppCacheInternalsJobFactory::IsSupportedURL(const GURL& url) {
  return StartsWithASCII(url.spec(),
                         chrome::kAppCacheViewInternalsURL,
                         true /*case_sensitive*/);
}

// static.
net::URLRequestJob* ViewAppCacheInternalsJobFactory::CreateJobForRequest(
    net::URLRequest* request) {
  net::URLRequestContext* context = request->context();
  ChromeURLRequestContext* chrome_request_context =
      reinterpret_cast<ChromeURLRequestContext*>(context);
  return new appcache::ViewAppCacheInternalsJob(
      request, chrome_request_context->appcache_service());
}
