// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_INTERCEPT_NAVIGATION_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_RENDERER_HOST_INTERCEPT_NAVIGATION_RESOURCE_THROTTLE_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

class GURL;

namespace content {
class RenderViewHost;
struct Referrer;
}

namespace net {
class URLRequest;
}

// This class allows the provider of the Callback to selectively ignore top
// level navigations.
class InterceptNavigationResourceThrottle : public content::ResourceThrottle {
 public:
  typedef base::Callback<bool(content::RenderViewHost* /* source */,
                              const GURL& /*url*/,
                              const content::Referrer& /*referrer*/,
                              bool /*is_content_initiated*/)>
      CheckOnUIThreadCallback;

  InterceptNavigationResourceThrottle(
      net::URLRequest* request,
      CheckOnUIThreadCallback should_ignore_callback);
  virtual ~InterceptNavigationResourceThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

 private:
  bool CheckIfShouldIgnoreNavigation(const GURL& url);
  void OnResultObtained(bool should_ignore_navigation);

  net::URLRequest* request_;
  CheckOnUIThreadCallback should_ignore_callback_;
  base::WeakPtrFactory<InterceptNavigationResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptNavigationResourceThrottle);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_INTERCEPT_NAVIGATION_RESOURCE_THROTTLE_H_
