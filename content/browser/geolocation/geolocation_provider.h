// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/geolocation_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/geolocation.h"
#include "content/public/common/geoposition.h"

class GeolocationArbitrator;
class GeolocationProviderTest;
template<typename Type> struct DefaultSingletonTraits;

// This is the main API to the geolocation subsystem. The application will hold
// a single instance of this class and can register multiple clients to be
// notified of location changes:
// * Observers are registered by AddObserver() and will keep receiving updates
//   until unregistered by RemoveObserver().
// * Callbacks are registered by RequestCallback() and will be called exactly
//   once when the next update becomes available.
// The application must instantiate the GeolocationProvider on the IO thread and
// must communicate with it on the same thread.
// The underlying location arbitrator will only be enabled whilst there is at
// least one registered observer or pending callback. The arbitrator and the
// location providers it uses run on a separate Geolocation thread.
class CONTENT_EXPORT GeolocationProvider
    : public base::Thread, public GeolocationObserver {
 public:
  // The GeolocationObserverOptions passed are used as a 'hint' for the provider
  // preferences for this particular observer, however the observer could
  // receive updates for best available locations from any active provider
  // whilst it is registered.
  // If an existing observer is added a second time, its options are updated
  // but only a single call to RemoveObserver() is required to remove it.
  void AddObserver(GeolocationObserver* delegate,
                   const GeolocationObserverOptions& update_options);

  // Remove a previously registered observer. No-op if not previously registered
  // via AddObserver(). Returns true if the observer was removed.
  bool RemoveObserver(GeolocationObserver* delegate);

  // Request a single callback when the next location update becomes available.
  // Callbacks must only be requested by code that is allowed to access the
  // location. No further permission checks will be made.
  void RequestCallback(const content::GeolocationUpdateCallback& callback);

  void OnPermissionGranted();
  bool HasPermissionBeenGranted() const;

  // GeolocationObserver implementation.
  virtual void OnLocationUpdate(const content::Geoposition& position) OVERRIDE;

  // Overrides the location for automation/testing. Suppresses any further
  // updates from the actual providers and sends an update with the overridden
  // position to all registered clients.
  void OverrideLocationForTesting(
      const content::Geoposition& override_position);

  // Gets a pointer to the singleton instance of the location relayer, which
  // is in turn bound to the browser's global context objects. This must only be
  // called on the IO thread so that the GeolocationProvider is always
  // instantiated on the same thread. Ownership is NOT returned.
  static GeolocationProvider* GetInstance();

 protected:
  friend struct DefaultSingletonTraits<GeolocationProvider>;
  GeolocationProvider();
  virtual ~GeolocationProvider();

 private:
  typedef std::map<GeolocationObserver*, GeolocationObserverOptions>
      ObserverMap;

  typedef std::vector<content::GeolocationUpdateCallback> CallbackList;

  bool OnGeolocationThread() const;

  // Start and stop providers as needed when clients are added or removed.
  void OnClientsChanged();

  // Stops the providers when there are no more registered clients. Note that
  // once the Geolocation thread is started, it will stay alive (but sitting
  // idle without any pending messages).
  void StopProviders();

  // Starts the geolocation providers or updates their options (delegates to
  // arbitrator).
  void StartProviders(const GeolocationObserverOptions& options);

  // Updates the providers on the geolocation thread, which must be running.
  void InformProvidersPermissionGranted();

  // Notifies all registered clients that a position update is available.
  void NotifyClients(const content::Geoposition& position);

  // Thread
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  // Only used on the IO thread
  ObserverMap observers_;
  CallbackList callbacks_;
  bool is_permission_granted_;
  content::Geoposition position_;

  // True only in testing, where we want to use a custom position.
  bool ignore_location_updates_;

  // Only to be used on the geolocation thread.
  GeolocationArbitrator* arbitrator_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationProvider);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_H_
