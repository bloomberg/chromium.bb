 // Copyright (c) 2012 The Chromium Authors. All rights reserved.
 // Use of this source code is governed by a BSD-style license that can be
 // found in the LICENSE file.

#include "chrome/browser/renderer_host/safe_browsing_resource_throttle_factory.h"
#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle.h"
#endif

using content::ResourceThrottle;

// static
SafeBrowsingResourceThrottleFactory*
    SafeBrowsingResourceThrottleFactory::factory_ = NULL;

// static
ResourceThrottle* SafeBrowsingResourceThrottleFactory::Create(
    net::URLRequest* request,
    int render_process_host_id,
    int render_view_id,
    bool is_subresource,
    SafeBrowsingService* service) {

#if defined(FULL_SAFE_BROWSING)
  return new SafeBrowsingResourceThrottle(request, render_process_host_id,
                 render_view_id, is_subresource, service);
#elif defined(MOBILE_SAFE_BROWSING)
  if (factory_)
    return factory_->CreateResourceThrottle(request, render_process_host_id,
                         render_view_id, is_subresource, service);
  return NULL;
#else
#error Need to define {FULL|MOBILE} SAFE_BROWSING mode.
#endif
}
