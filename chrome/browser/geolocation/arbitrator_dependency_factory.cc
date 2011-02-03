// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/arbitrator_dependency_factory.h"

#include "chrome/browser/geolocation/access_token_store.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/profiles/profile.h"

// GeolocationArbitratorDependencyFactory
GeolocationArbitratorDependencyFactory::
~GeolocationArbitratorDependencyFactory() {
}

// DefaultGeolocationArbitratorDependencyFactory
URLRequestContextGetter*
DefaultGeolocationArbitratorDependencyFactory::GetContextGetter() {
  return Profile::GetDefaultRequestContext();
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
    URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
  return ::NewNetworkLocationProvider(access_token_store, context,
                                      url, access_token);
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewSystemLocationProvider() {
  return ::NewSystemLocationProvider();
}

