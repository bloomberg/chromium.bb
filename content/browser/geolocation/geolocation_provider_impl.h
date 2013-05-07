// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/common/geoposition.h"

template<typename Type> struct DefaultSingletonTraits;

namespace content {
class GeolocationArbitrator;
class GeolocationProviderTest;

// This is the main API to the geolocation subsystem. The application will hold
// a single instance of this class and can register multiple clients to be
// notified of location changes:
// * Observers are registered by AddLocationUpdateCallback() and will keep
//   receiving updates
//   until unregistered by RemoveLocationUpdateCallback().
// * Callbacks are registered by RequestCallback() and will be called exactly
//   once when the next update becomes available.
// The application must instantiate the GeolocationProvider on the IO thread and
// must communicate with it on the same thread.
// The underlying location arbitrator will only be enabled whilst there is at
// least one registered observer or pending callback. The arbitrator and the
// location providers it uses run on a separate Geolocation thread.
class CONTENT_EXPORT GeolocationProviderImpl
    : public NON_EXPORTED_BASE(GeolocationProvider),
      public base::Thread {
 public:
  // GeolocationProvider implementation:
  virtual void AddLocationUpdateCallback(const LocationUpdateCallback& callback,
                                         bool use_high_accuracy) OVERRIDE;
  virtual bool RemoveLocationUpdateCallback(
      const LocationUpdateCallback& callback) OVERRIDE;
  virtual void UserDidOptIntoLocationServices() OVERRIDE;

  bool LocationServicesOptedIn() const;

  // Overrides the location for automation/testing. Suppresses any further
  // updates from the actual providers and sends an update with the overridden
  // position to all registered clients.
  void OverrideLocationForTesting(const Geoposition& override_position);

  // Callback from the GeolocationArbitrator. Public for testing.
  void OnLocationUpdate(const Geoposition& position);

  // Gets a pointer to the singleton instance of the location relayer, which
  // is in turn bound to the browser's global context objects. This must only be
  // called on the IO thread so that the GeolocationProviderImpl is always
  // instantiated on the same thread. Ownership is NOT returned.
  static GeolocationProviderImpl* GetInstance();

 protected:
  friend struct DefaultSingletonTraits<GeolocationProviderImpl>;
  GeolocationProviderImpl();
  virtual ~GeolocationProviderImpl();

  // Useful for injecting mock geolocation arbitrator in tests.
  virtual GeolocationArbitrator* CreateArbitrator();

 private:
  typedef std::pair<LocationUpdateCallback, bool> LocationUpdateInfo;
  typedef std::list<LocationUpdateInfo> CallbackList;

  bool OnGeolocationThread() const;

  // Start and stop providers as needed when clients are added or removed.
  void OnClientsChanged();

  // Stops the providers when there are no more registered clients. Note that
  // once the Geolocation thread is started, it will stay alive (but sitting
  // idle without any pending messages).
  void StopProviders();

  // Starts the geolocation providers or updates their options (delegates to
  // arbitrator).
  void StartProviders(bool use_high_accuracy);

  // Updates the providers on the geolocation thread, which must be running.
  void InformProvidersPermissionGranted();

  // Notifies all registered clients that a position update is available.
  void NotifyClients(const Geoposition& position);

  // Thread
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  // Only used on the IO thread
  CallbackList callbacks_;
  bool user_did_opt_into_location_services_;
  Geoposition position_;

  // True only in testing, where we want to use a custom position.
  bool ignore_location_updates_;

  // Only to be used on the geolocation thread.
  GeolocationArbitrator* arbitrator_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
