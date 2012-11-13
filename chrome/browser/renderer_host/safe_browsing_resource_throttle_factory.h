// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_

#include "base/basictypes.h"

class SafeBrowsingService;

namespace content {
class ResourceThrottle;
}

namespace net {
class URLRequest;
}

// Factory for creating a SafeBrowsingResourceThrottle. There can only be
// one type of resource throttle factory for a given product. To create
// specialized resource throttles for different products, provide a
// definition of this class for this product only using build time
// configuration.
class SafeBrowsingResourceThrottleFactory {
 public:
  // Creates a new resource throttle for safe browsing
  static content::ResourceThrottle* Create(
      net::URLRequest* request,
      int render_process_host_id,
      int render_view_id,
      bool is_subresource,
      SafeBrowsingService* service);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SafeBrowsingResourceThrottleFactory);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_FACTORY_H_
