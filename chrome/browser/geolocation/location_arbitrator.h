// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
#define CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_

#include "base/ref_counted.h"

class AccessTokenStore;
class LocationProviderBase;
class URLRequestContextGetter;
struct Geoposition;

// This is the main API to the geolocaiton subsystem. Typically the application
// will hold a single instance of this class, and can register multiple
// observers which will be notified of location updates. Underlying location
// provider(s) will only be enabled whilst there is at least one observer
// registered.
// This class is responsible for handling updates from multiple underlying
// providers and resolving them to a single 'best' location fix at any given
// moment.
class GeolocationArbitrator : public base::RefCounted<GeolocationArbitrator> {
 public:
  // Creates and returns a new instance of the location arbitrator. Allows
  // injection of the access token store and url getter context, for testing.
  static GeolocationArbitrator* Create(
      AccessTokenStore* access_token_store,
      URLRequestContextGetter* context_getter);

  // Gets a pointer to the singleton instance of the location arbitrator, which
  // is in turn bound to the browser's global context objects. Ownership is NOT
  // returned.
  static GeolocationArbitrator* GetInstance();

  class Delegate {
   public:
    // This will be called whenever the 'best available' location is updated,
    // or when an error is encountered meaning no location data will be
    // available in the forseeable future.
    virtual void OnLocationUpdate(const Geoposition& position) = 0;

   protected:
    virtual ~Delegate() {}
  };
  struct UpdateOptions {
    UpdateOptions() : use_high_accuracy(false) {}
    explicit UpdateOptions(bool high_accuracy)
        : use_high_accuracy(high_accuracy) {}
    bool use_high_accuracy;
  };

  // Must be called from the same thread as the arbitrator was created on.
  // The update options passed are used as a 'hint' for the provider preferences
  // for this particular observer, however the delegate could receive callbacks
  // for best available locations from any active provider whilst it is
  // registerd.
  // If an existing delegate is added a second time it's options are updated
  // but only a single call to RemoveObserver() is required to remove it.
  virtual void AddObserver(Delegate* delegate,
                           const UpdateOptions& update_options) = 0;
  // Remove a previously registered observer. No-op if not previously registered
  // via AddObserver(). Returns true if the observer was removed.
  virtual bool RemoveObserver(Delegate* delegate) = 0;

  // For testing, a factory functino can be set which will be used to create
  // a specified test provider. Pass NULL to reset to the default behavior.
  typedef LocationProviderBase* (*LocationProviderFactoryFunction)(void);
  static void SetProviderFactoryForTest(
      LocationProviderFactoryFunction factory_function);

 protected:
  friend class base::RefCounted<GeolocationArbitrator>;
  GeolocationArbitrator();
  // RefCounted object; no not delete directly.
  virtual ~GeolocationArbitrator();

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationArbitrator);
};

#endif  // CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
