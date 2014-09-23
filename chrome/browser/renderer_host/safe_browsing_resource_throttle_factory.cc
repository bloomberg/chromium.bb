 // Copyright (c) 2012 The Chromium Authors. All rights reserved.
 // Use of this source code is governed by a BSD-style license that can be
 // found in the LICENSE file.

#include "chrome/browser/renderer_host/safe_browsing_resource_throttle_factory.h"

#include "content/public/browser/resource_context.h"
#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle.h"
#endif

using content::ResourceThrottle;

// static
SafeBrowsingResourceThrottleFactory*
    SafeBrowsingResourceThrottleFactory::factory_ = NULL;

// static
void SafeBrowsingResourceThrottleFactory::RegisterFactory(
    SafeBrowsingResourceThrottleFactory* factory) {
  factory_ = factory;
}

// static
ResourceThrottle* SafeBrowsingResourceThrottleFactory::Create(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceType resource_type,
    SafeBrowsingService* service) {
  if (factory_)
    return factory_->CreateResourceThrottle(
        request, resource_context, resource_type, service);

#if defined(FULL_SAFE_BROWSING)
  return new SafeBrowsingResourceThrottle(request, resource_type, service);
#elif defined(MOBILE_SAFE_BROWSING)
  return NULL;
#else
#error Need to define {FULL|MOBILE} SAFE_BROWSING mode.
#endif
}
