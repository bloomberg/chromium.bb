// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_ARBITRATOR_DEPENDENCY_FACTORY_H_
#define CONTENT_BROWSER_GEOLOCATION_ARBITRATOR_DEPENDENCY_FACTORY_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/common/content_export.h"

class AccessTokenStore;
class GURL;
class LocationProviderBase;

namespace base {
class Time;
}

namespace net {
class URLRequestContextGetter;
}

// Allows injection of factory methods for creating the location providers.
// RefCounted for simplicity of writing tests.
class CONTENT_EXPORT GeolocationArbitratorDependencyFactory
    : public base::RefCounted<GeolocationArbitratorDependencyFactory> {
 public:
  // Defines a function that returns the current time.
  typedef base::Time (*GetTimeNow)();

  virtual GetTimeNow GetTimeFunction() = 0;
  virtual AccessTokenStore* NewAccessTokenStore() = 0;
  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      net::URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token) = 0;
  virtual LocationProviderBase* NewSystemLocationProvider() = 0;

 protected:
  friend class base::RefCounted<GeolocationArbitratorDependencyFactory>;
  virtual ~GeolocationArbitratorDependencyFactory();
};

// The default dependency factory, exposed so that it is possible
// to override only certain parts (e.g. the location providers).
class CONTENT_EXPORT DefaultGeolocationArbitratorDependencyFactory
    : public GeolocationArbitratorDependencyFactory {
 public:
  // GeolocationArbitratorDependencyFactory
  virtual GetTimeNow GetTimeFunction() OVERRIDE;
  virtual AccessTokenStore* NewAccessTokenStore() OVERRIDE;
  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      net::URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token) OVERRIDE;
  virtual LocationProviderBase* NewSystemLocationProvider() OVERRIDE;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_ARBITRATOR_DEPENDENCY_FACTORY_H_
