// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
#define CHROME_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
#pragma once

#include "base/string16.h"
#include "base/time.h"
#include "base/ref_counted.h"

class AccessTokenStore;
class GURL;
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
  // Number of milliseconds newer a location provider has to be that it's worth
  // switching to this location provider on the basis of it being fresher
  // (regardles of relative accuracy). Public for tests.
  static const int64 kFixStaleTimeoutMilliseconds;

  // Defines a function that returns the current time.
  typedef base::Time (*GetTimeNow)();

  // Allows injection of factory methods for creating the location providers.
  // RefCounted for simplicity of writing tests.
  class ProviderFactory  : public base::RefCounted<ProviderFactory> {
   public:
    virtual LocationProviderBase* NewNetworkLocationProvider(
        AccessTokenStore* access_token_store,
        URLRequestContextGetter* context,
        const GURL& url,
        const string16& access_token) = 0;
    virtual LocationProviderBase* NewSystemLocationProvider() = 0;

   protected:
    friend class base::RefCounted<ProviderFactory>;
    virtual ~ProviderFactory();
  };

  // Creates and returns a new instance of the location arbitrator. Allows
  // injection of dependencies, for testing.
  static GeolocationArbitrator* Create(
      AccessTokenStore* access_token_store,
      URLRequestContextGetter* context_getter,
      GetTimeNow get_time_now,
      ProviderFactory* provider_factory);

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
    Delegate() {}
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };
  struct UpdateOptions {
    UpdateOptions() : use_high_accuracy(false) {}
    explicit UpdateOptions(bool high_accuracy)
        : use_high_accuracy(high_accuracy) {}

    // Given a map<ANYTHING, UpdateOptions> this function will iterate the map
    // and collapse all the options found to a single instance that satisfies
    // them all.
    template <typename MAP>
    static UpdateOptions Collapse(const MAP& options_map) {
      for (typename MAP::const_iterator it = options_map.begin();
           it != options_map.end(); ++it) {
        if (it->second.use_high_accuracy)
          return UpdateOptions(true);
      }
      return UpdateOptions(false);
    }

    bool use_high_accuracy;
  };

  // Must be called from the same thread as the arbitrator was created on.
  // The update options passed are used as a 'hint' for the provider preferences
  // for this particular observer, however the delegate could receive callbacks
  // for best available locations from any active provider whilst it is
  // registered.
  // If an existing delegate is added a second time it's options are updated
  // but only a single call to RemoveObserver() is required to remove it.
  virtual void AddObserver(Delegate* delegate,
                           const UpdateOptions& update_options) = 0;
  // Remove a previously registered observer. No-op if not previously registered
  // via AddObserver(). Returns true if the observer was removed.
  virtual bool RemoveObserver(Delegate* delegate) = 0;

  // Returns the current position estimate, or an uninitialized position
  // if none is yet available. Once initialized, this will always match
  // the most recent observer notification (via Delegate::OnLocationUpdate()).
  virtual Geoposition GetCurrentPosition() = 0;

  // Called everytime permission is granted to a page for using geolocation.
  // This may either be through explicit user action (e.g. responding to the
  // infobar prompt) or inferred from a persisted site permission.
  // The arbitrator will inform all providers of this, which may in turn use
  // this information to modify their internal policy.
  virtual void OnPermissionGranted(const GURL& requesting_frame) = 0;

  // Returns true if this arbitrator has received at least one call to
  // OnPermissionGranted().
  virtual bool HasPermissionBeenGranted() const = 0;

  // For testing, a factory function can be set which will be used to create
  // a specified test provider. Pass NULL to reset to the default behavior.
  // For finer grained control, use class ProviderFactory instead.
  // TODO(joth): Move all tests to use ProviderFactory and remove this.
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
