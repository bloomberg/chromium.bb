// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_

#include "base/basictypes.h"

class SafeBrowsingService;

namespace content {
class ResourceContext;
class ResourceThrottle;
}

namespace net {
class URLRequest;
}

// Factory for creating a SafeBrowsingResourceThrottle. When FULL_SAFE_BROWSING
// is enabled, creates a SafeBrowsingResourceThrottle. When MOBILE_SAFE_BROWSING
// is enabled, the default implementation creates a null resource throttle,
// therefore, a factory has to be registered before using this.
class SafeBrowsingResourceThrottleFactory {
 public:
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  // Registers a factory. Does not take the ownership of the factory. The
  // caller has to make sure the factory stays alive and properly destroyed.
  static void RegisterFactory(SafeBrowsingResourceThrottleFactory* factory) {
    factory_ = factory;
  }
#endif

  // Creates a new resource throttle for safe browsing
  static content::ResourceThrottle* Create(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      bool is_subresource,
      SafeBrowsingService* service);

 protected:
  SafeBrowsingResourceThrottleFactory() { }
  virtual ~SafeBrowsingResourceThrottleFactory() { }

  virtual content::ResourceThrottle* CreateResourceThrottle(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      bool is_subresource,
      SafeBrowsingService* service) = 0;

 private:
  static SafeBrowsingResourceThrottleFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceThrottleFactory);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_
