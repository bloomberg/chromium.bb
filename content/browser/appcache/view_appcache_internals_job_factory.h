// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_FACTORY_H_
#define CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_FACTORY_H_
#pragma once

class ChromeAppCacheService;
class GURL;
namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

class ViewAppCacheInternalsJobFactory {
 public:
  static bool IsSupportedURL(const GURL& url);
  static net::URLRequestJob* CreateJobForRequest(
      net::URLRequest* request,
      ChromeAppCacheService* appcache_service);
};

#endif  // CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_FACTORY_H_
