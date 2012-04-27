// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/arbitrator_dependency_factories_for_test.h"

using content::AccessTokenStore;

GeolocationArbitratorDependencyFactoryWithLocationProvider::
    GeolocationArbitratorDependencyFactoryWithLocationProvider(
    LocationProviderFactoryFunction factory_function)
    : factory_function_(factory_function) {
}

LocationProviderBase*
GeolocationArbitratorDependencyFactoryWithLocationProvider::
NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
  return factory_function_();
}

LocationProviderBase*
GeolocationArbitratorDependencyFactoryWithLocationProvider::
NewSystemLocationProvider() {
  return NULL;
}

GeolocationArbitratorDependencyFactoryWithLocationProvider::
~GeolocationArbitratorDependencyFactoryWithLocationProvider() {
}

