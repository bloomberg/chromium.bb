// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_GEOFENCING_MANAGER_H_
#define CONTENT_BROWSER_GEOFENCING_GEOFENCING_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/geofencing_status.h"

template <typename T>
struct DefaultSingletonTraits;
class GURL;

namespace blink {
struct WebCircularGeofencingRegion;
};

namespace content {

class BrowserContext;
class GeofencingProvider;

// This is the main API to the geofencing subsystem. The application will hold
// a single instance of this class.
// This class is responsible for keeping track of which geofences are currently
// registered by websites/workers, persisting this list of registrations and
// registering a subset of these active registrations with the underlying
// platform specific |GeofencingProvider| instance.
// This class lives on the IO thread, and all public methods of it should only
// ever be called from that same thread.
// FIXME: Does it make more sense for this to live on the UI thread instead of
//     the IO thread?
// TODO(mek): Implement some kind of persistence of registrations.
// TODO(mek): Limit the number of geofences that are registered with the
//    underlying GeofencingProvider.
class CONTENT_EXPORT GeofencingManager {
 public:
  typedef base::Callback<void(GeofencingStatus)> StatusCallback;
  typedef base::Callback<void(
      GeofencingStatus,
      const std::map<std::string, blink::WebCircularGeofencingRegion>&)>
      RegistrationsCallback;

  // Gets a pointer to the singleton instance of the geofencing manager. This
  // must only be called on the IO thread so that the GeofencingManager is
  // always instantiated on the same thread. Ownership is NOT returned.
  static GeofencingManager* GetInstance();

  // Initiates registration of a new geofence. StatusCallback is called when
  // registration has completed or failed (which could possibly be before
  // RegisterRegion returns.
  // Attempting to register a region with the same ID as an already registered
  // (or in progress of being registered) region will fail.
  // TODO(mek): Behavior when using an already used ID might need to be revised
  //     depending on what the actual spec ends up saying about this.
  void RegisterRegion(BrowserContext* browser_context,
                      int64 service_worker_registration_id,
                      const GURL& service_worker_origin,
                      const std::string& region_id,
                      const blink::WebCircularGeofencingRegion& region,
                      const StatusCallback& callback);

  // Unregister a region that was previously registered by a call to
  // RegisterRegion. Any attempt to unregister a region that has not been
  // registered, or for which the registration is still in progress
  // (RegisterRegion hasn't called its callback yet) will fail.
  // TODO(mek): Maybe better behavior would be to allow unregistering still
  //     in-progress registrations.
  void UnregisterRegion(BrowserContext* browser_context,
                        int64 service_worker_registration_id,
                        const GURL& service_worker_origin,
                        const std::string& region_id,
                        const StatusCallback& callback);

  // Returns all currently registered regions. In case of failure (no geofencing
  // provider available for example) return an error status, while leaving
  // |regions| untouched.
  // This only returns regions for which the callback passed to RegisterRegion
  // has been called already (so it doesn't include still in progress
  // registrations).
  GeofencingStatus GetRegisteredRegions(
      BrowserContext* browser_context,
      int64 service_worker_registration_id,
      const GURL& service_worker_origin,
      std::map<std::string, blink::WebCircularGeofencingRegion>* regions);

  void SetProviderForTests(scoped_ptr<GeofencingProvider> provider);

 protected:
  friend struct DefaultSingletonTraits<GeofencingManager>;
  friend class GeofencingManagerTest;
  GeofencingManager();
  virtual ~GeofencingManager();

 private:
  // Internal bookkeeping associated with each registered geofence.
  struct RegistrationKey;
  struct Registration;
  class RegistrationMatches;

  // Called by GeofencingProvider when the platform specific provider completes
  // registration of a geofence.
  void RegisterRegionCompleted(const StatusCallback& callback,
                               const RegistrationKey& key,
                               GeofencingStatus status,
                               int registration_id);

  // Looks up a particular geofence registration. Returns nullptr if no
  // registration with the given IDs exists.
  Registration* FindRegistration(const RegistrationKey& key);

  // Registers a new registration, returning a reference to the newly inserted
  // object. Assumes no registration with the same IDs currently exists.
  Registration& AddRegistration(
      const RegistrationKey& key,
      const blink::WebCircularGeofencingRegion& region);

  // Clears a registration.
  void ClearRegistration(const RegistrationKey& key);

  // List of all currently registered geofences.
  // TODO(mek): Better way of storing these that allows more efficient lookup
  //     and deletion.
  std::vector<Registration> registrations_;

  scoped_ptr<GeofencingProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(GeofencingManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_GEOFENCING_MANAGER_H_
