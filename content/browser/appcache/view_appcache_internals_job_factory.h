// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_FACTORY_H_
#define CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_FACTORY_H_
#pragma once

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

class GURL;

class ViewAppCacheInternalsJobFactory {
 public:
  static bool IsSupportedURL(const GURL& url);
  static net::URLRequestJob* CreateJobForRequest(net::URLRequest* request);
};

#endif  // CONTENT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_FACTORY_H_
