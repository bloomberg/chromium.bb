// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/safe_browsing_resource_throttle_factory.h"

// Compiling this file only makes sense if SAFE_BROWSING_SERVICE is enabled.
// If the build is breaking here, it probably means that a gyp or gn file has
// been modified to build this file with safe_browsing=0 (gn
// safe_browsing_mode=0).
#if !defined(SAFE_BROWSING_SERVICE)
#error Need to define safe_browsing mode.
#endif

#if defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle.h"
#endif

using content::ResourceThrottle;

namespace {

SafeBrowsingResourceThrottleFactory* g_factory = nullptr;

}  // namespace

// static
void SafeBrowsingResourceThrottleFactory::RegisterFactory(
    SafeBrowsingResourceThrottleFactory* factory) {
  g_factory = factory;
}

// static
ResourceThrottle* SafeBrowsingResourceThrottleFactory::Create(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceType resource_type,
    SafeBrowsingService* service) {
  if (g_factory) {
    return g_factory->CreateResourceThrottle(
        request, resource_context, resource_type, service);
  }
  return CreateWithoutRegisteredFactory(request, resource_type, service);
}

ResourceThrottle*
SafeBrowsingResourceThrottleFactory::CreateWithoutRegisteredFactory(
    net::URLRequest* request,
    content::ResourceType resource_type,
    SafeBrowsingService* service) {
#if defined(SAFE_BROWSING_DB_LOCAL)
  // Throttle consults a local database before starting the resource request.
  return new SafeBrowsingResourceThrottle(request, resource_type, service,
                                          true /* defer_at_start */);
#elif defined(SAFE_BROWSING_DB_REMOTE)
  // Throttle consults a remote database before processing the response.
  return new SafeBrowsingResourceThrottle(request, resource_type, service,
                                          false /* defer_at_start */);
#else
  return nullptr;
#endif
}
