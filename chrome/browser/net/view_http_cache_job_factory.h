// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_VIEW_HTTP_CACHE_JOB_FACTORY_H_
#define CHROME_BROWSER_NET_VIEW_HTTP_CACHE_JOB_FACTORY_H_

class GURL;
class URLRequest;
class URLRequestJob;

class ViewHttpCacheJobFactory {
 public:
  static bool IsSupportedURL(const GURL& url);
  static URLRequestJob* CreateJobForRequest(URLRequest* request);
};

#endif  // CHROME_BROWSER_NET_VIEW_HTTP_CACHE_JOB_FACTORY_H_
