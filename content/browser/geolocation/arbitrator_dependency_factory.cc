// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/arbitrator_dependency_factory.h"

#include "base/time.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/content_browser_client.h"

using content::AccessTokenStore;

// GeolocationArbitratorDependencyFactory
GeolocationArbitratorDependencyFactory::
~GeolocationArbitratorDependencyFactory() {
}

// DefaultGeolocationArbitratorDependencyFactory
DefaultGeolocationArbitratorDependencyFactory::GetTimeNow
DefaultGeolocationArbitratorDependencyFactory::GetTimeFunction() {
  return base::Time::Now;
}

AccessTokenStore*
DefaultGeolocationArbitratorDependencyFactory::NewAccessTokenStore() {
  return content::GetContentClient()->browser()->CreateAccessTokenStore();
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
#if defined(OS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return NULL;
#else
  return ::NewNetworkLocationProvider(access_token_store, context,
                                      url, access_token);
#endif
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewSystemLocationProvider() {
  return ::NewSystemLocationProvider();
}

DefaultGeolocationArbitratorDependencyFactory::
~DefaultGeolocationArbitratorDependencyFactory() {
}
