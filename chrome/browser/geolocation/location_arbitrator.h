// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
#define CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
#pragma once

#include "base/string16.h"
#include "base/scoped_vector.h"
#include "base/time.h"
#include "chrome/browser/geolocation/access_token_store.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/geolocation/geolocation_observer.h"
#include "chrome/common/geoposition.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"

class AccessTokenStore;
class GeolocationArbitratorDependencyFactory;
class GURL;
class LocationProviderBase;
class URLRequestContextGetter;
struct Geoposition;

// This class is responsible for handling updates from multiple underlying
// providers and resolving them to a single 'best' location fix at any given
// moment.
class GeolocationArbitrator : public LocationProviderBase::ListenerInterface {
 public:
  // Number of milliseconds newer a location provider has to be that it's worth
  // switching to this location provider on the basis of it being fresher
  // (regardles of relative accuracy). Public for tests.
  static const int64 kFixStaleTimeoutMilliseconds;

  // Defines a function that returns the current time.
  typedef base::Time (*GetTimeNow)();

  ~GeolocationArbitrator();

  static GeolocationArbitrator* Create(GeolocationObserver* observer);

  // See more details in geolocation_provider.
  void StartProviders(const GeolocationObserverOptions& options);
  void StopProviders();

  // Called everytime permission is granted to a page for using geolocation.
  // This may either be through explicit user action (e.g. responding to the
  // infobar prompt) or inferred from a persisted site permission.
  // The arbitrator will inform all providers of this, which may in turn use
  // this information to modify their internal policy.
  void OnPermissionGranted(const GURL& requesting_frame);

  // Returns true if this arbitrator has received at least one call to
  // OnPermissionGranted().
  bool HasPermissionBeenGranted() const;

  // Call this function every time you need to create an specially parameterised
  // arbitrator.
  static void SetDependencyFactoryForTest(
      GeolocationArbitratorDependencyFactory* factory);

  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider);

 private:
  GeolocationArbitrator(
      GeolocationArbitratorDependencyFactory* dependency_factory,
      GeolocationObserver* observer);
  // Takes ownership of |provider| on entry; it will either be added to
  // |providers_| or deleted on error (e.g. it fails to start).
  void RegisterProvider(LocationProviderBase* provider);
  void OnAccessTokenStoresLoaded(
      AccessTokenStore::AccessTokenSet access_token_store);
  void StartProviders();
  // Returns true if |new_position| is an improvement over |old_position|.
  // Set |from_same_provider| to true if both the positions came from the same
  // provider.
  bool IsNewPositionBetter(const Geoposition& old_position,
                           const Geoposition& new_position,
                           bool from_same_provider) const;

  scoped_refptr<GeolocationArbitratorDependencyFactory> dependency_factory_;
  scoped_refptr<AccessTokenStore> access_token_store_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  GetTimeNow get_time_now_;
  GeolocationObserver* observer_;
  ScopedVector<LocationProviderBase> providers_;
  GeolocationObserverOptions current_provider_options_;
  // The provider which supplied the current |position_|
  const LocationProviderBase* position_provider_;
  GURL most_recent_authorized_frame_;
  CancelableRequestConsumer request_consumer_;
  // The current best estimate of our position.
  Geoposition position_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationArbitrator);
};

#endif  // CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
