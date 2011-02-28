// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_ARBITRATOR_DEPENDENCY_FACTORIES_FOR_TEST_H_
#define CONTENT_BROWSER_GEOLOCATION_ARBITRATOR_DEPENDENCY_FACTORIES_FOR_TEST_H_
#pragma once

#include "content/browser/geolocation/arbitrator_dependency_factory.h"

class GeolocationArbitratorDependencyFactoryWithLocationProvider
    : public DefaultGeolocationArbitratorDependencyFactory {
 public:
  typedef LocationProviderBase* (*LocationProviderFactoryFunction)(void);

  GeolocationArbitratorDependencyFactoryWithLocationProvider(
      LocationProviderFactoryFunction factory_function);
  virtual ~GeolocationArbitratorDependencyFactoryWithLocationProvider();

  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token);

  virtual LocationProviderBase* NewSystemLocationProvider();

 protected:
  LocationProviderFactoryFunction factory_function_;
};


#endif  // CONTENT_BROWSER_GEOLOCATION_ARBITRATOR_DEPENDENCY_FACTORIES_FOR_TEST_H_
