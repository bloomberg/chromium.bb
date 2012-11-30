// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_IMPL_H_

#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/time.h"
#include "content/browser/geolocation/geolocation_observer.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/common/geoposition.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {
class AccessTokenStore;
class LocationProviderBase;

// This class is responsible for handling updates from multiple underlying
// providers and resolving them to a single 'best' location fix at any given
// moment.
class CONTENT_EXPORT GeolocationArbitratorImpl
    : public GeolocationArbitrator,
      public LocationProviderBase::ListenerInterface {
 public:
  // Number of milliseconds newer a location provider has to be that it's worth
  // switching to this location provider on the basis of it being fresher
  // (regardles of relative accuracy). Public for tests.
  static const int64 kFixStaleTimeoutMilliseconds;

  explicit GeolocationArbitratorImpl(GeolocationObserver* observer);
  virtual ~GeolocationArbitratorImpl();

  static GURL DefaultNetworkProviderURL();

  // GeolocationArbitrator
  virtual void StartProviders(const GeolocationObserverOptions& options)
      OVERRIDE;
  virtual void StopProviders() OVERRIDE;
  virtual void OnPermissionGranted() OVERRIDE;
  virtual bool HasPermissionBeenGranted() const OVERRIDE;

  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider) OVERRIDE;

 protected:

  AccessTokenStore* GetAccessTokenStore();

  // These functions are useful for injection of dependencies in derived
  // testing classes.
  virtual AccessTokenStore* NewAccessTokenStore();
  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      net::URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token);
  virtual LocationProviderBase* NewSystemLocationProvider();
  virtual base::Time GetTimeNow() const;

 private:
  // Takes ownership of |provider| on entry; it will either be added to
  // |providers_| or deleted on error (e.g. it fails to start).
  void RegisterProvider(LocationProviderBase* provider);
  void OnAccessTokenStoresLoaded(
      AccessTokenStore::AccessTokenSet access_token_store,
      net::URLRequestContextGetter* context_getter);
  void DoStartProviders();
  // Returns true if |new_position| is an improvement over |old_position|.
  // Set |from_same_provider| to true if both the positions came from the same
  // provider.
  bool IsNewPositionBetter(const Geoposition& old_position,
                           const Geoposition& new_position,
                           bool from_same_provider) const;

  scoped_refptr<AccessTokenStore> access_token_store_;
  GeolocationObserver* observer_;
  ScopedVector<LocationProviderBase> providers_;
  GeolocationObserverOptions current_provider_options_;
  // The provider which supplied the current |position_|
  const LocationProviderBase* position_provider_;
  bool is_permission_granted_;
  // The current best estimate of our position.
  Geoposition position_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationArbitratorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_IMPL_H_
