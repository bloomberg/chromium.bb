// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_RESOURCE_THROTTLE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_RESOURCE_THROTTLE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

class GURL;

namespace content {
class WebContents;
}

namespace net {
class URLRequest;
}

namespace navigation_interception {

class NavigationParams;

// This class allows the provider of the Callback to selectively ignore top
// level navigations.
class InterceptNavigationResourceThrottle : public content::ResourceThrottle {
 public:
  typedef base::Callback<bool(
          content::WebContents* /* source */,
          const NavigationParams& /* navigation_params */)>
      CheckOnUIThreadCallback;

  InterceptNavigationResourceThrottle(
      net::URLRequest* request,
      CheckOnUIThreadCallback should_ignore_callback);
  virtual ~InterceptNavigationResourceThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

 private:
  std::string GetMethodAfterRedirect();
  bool CheckIfShouldIgnoreNavigation(const GURL& url,
                                     const std::string& method,
                                     bool is_redirect);
  void OnResultObtained(bool should_ignore_navigation);

  net::URLRequest* request_;
  CheckOnUIThreadCallback should_ignore_callback_;
  base::WeakPtrFactory<InterceptNavigationResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptNavigationResourceThrottle);
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_RESOURCE_THROTTLE_H_
