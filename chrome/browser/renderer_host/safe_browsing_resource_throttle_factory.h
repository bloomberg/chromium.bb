// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_

#include "base/basictypes.h"
#include "content/public/common/resource_type.h"

class SafeBrowsingService;

namespace content {
class ResourceContext;
class ResourceThrottle;
}

namespace net {
class URLRequest;
}

// Factory for creating a SafeBrowsingResourceThrottle. If a factory is
// registered, the factory's CreateResourceThrottle() is called.  Otherwise,
// if SAFE_BROWSING_DB_LOCAL || SAFE_BROWSING_DB_REMOTE is enabled, a new
// SafeBrowsingResourceThrottle is returned, or NULL if only
// SAFE_BROWSING_SERVICE is enabled.
class SafeBrowsingResourceThrottleFactory {
 public:
  // Registers a factory. Does not take the ownership of the factory. The
  // caller has to make sure the factory stays alive and properly destroyed.
  static void RegisterFactory(SafeBrowsingResourceThrottleFactory* factory);

  // Creates a new resource throttle for safe browsing, using the factory
  // if there is one registered.
  static content::ResourceThrottle* Create(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceType resource_type,
      SafeBrowsingService* service);

  // Like above, but bypass the registered factory and create a true
  // SafeBrowsingResourceThrottle.
  static content::ResourceThrottle* CreateWithoutRegisteredFactory(
      net::URLRequest* request,
      content::ResourceType resource_type,
      SafeBrowsingService* service);

 protected:
  SafeBrowsingResourceThrottleFactory() { }
  virtual ~SafeBrowsingResourceThrottleFactory() { }

  virtual content::ResourceThrottle* CreateResourceThrottle(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceType resource_type,
      SafeBrowsingService* service) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceThrottleFactory);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_
