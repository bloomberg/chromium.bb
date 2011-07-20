// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/arbitrator_dependency_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "content/browser/geolocation/access_token_store.h"
#include "content/browser/geolocation/location_provider.h"

// GeolocationArbitratorDependencyFactory
GeolocationArbitratorDependencyFactory::
~GeolocationArbitratorDependencyFactory() {
}

// DefaultGeolocationArbitratorDependencyFactory
net::URLRequestContextGetter*
DefaultGeolocationArbitratorDependencyFactory::GetContextGetter() {
  return Profile::Deprecated::GetDefaultRequestContext();
}

DefaultGeolocationArbitratorDependencyFactory::GetTimeNow
DefaultGeolocationArbitratorDependencyFactory::GetTimeFunction() {
  return base::Time::Now;
}

AccessTokenStore*
DefaultGeolocationArbitratorDependencyFactory::NewAccessTokenStore() {
  return NewChromePrefsAccessTokenStore();
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
  return ::NewNetworkLocationProvider(access_token_store, context,
                                      url, access_token);
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewSystemLocationProvider() {
  return ::NewSystemLocationProvider();
}
