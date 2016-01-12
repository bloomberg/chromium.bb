// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_GEOFENCING_MANAGER_H_
#define CONTENT_BROWSER_GEOFENCING_GEOFENCING_MANAGER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/geofencing/geofencing_registration_delegate.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "content/common/geofencing_types.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base  // namespace base

class GURL;

namespace blink {
struct WebCircularGeofencingRegion;
};

namespace content {

class GeofencingService;
class MockGeofencingService;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

// This is the main API to the geofencing subsystem. There is one instance of
// this class per storage partition.
// This class is responsible for keeping track of which geofences are currently
// registered by websites/workers, persisting this list of registrations and
// registering them with the global |GeofencingService|.
// This class is created on the UI thread, but all its methods should only be
// called from the IO thread.
// TODO(mek): Implement some kind of persistence of registrations.
class CONTENT_EXPORT GeofencingManager
    : NON_EXPORTED_BASE(public GeofencingRegistrationDelegate),
      NON_EXPORTED_BASE(public ServiceWorkerContextObserver),
      public base::RefCountedThreadSafe<GeofencingManager> {
 public:
  typedef base::Callback<void(GeofencingStatus)> StatusCallback;

  explicit GeofencingManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // Init and Shutdown are for use on the UI thread when the storagepartition is
  // being setup and torn down.
  void Init();
  void Shutdown();

  // Initiates registration of a new geofence. StatusCallback is called when
  // registration has completed or failed (which could possibly be before
  // RegisterRegion returns.
  // Attempting to register a region with the same ID as an already registered
  // (or in progress of being registered) region will fail.
  // TODO(mek): Behavior when using an already used ID might need to be revised
  //     depending on what the actual spec ends up saying about this.
  void RegisterRegion(int64_t service_worker_registration_id,
                      const std::string& region_id,
                      const blink::WebCircularGeofencingRegion& region,
                      const StatusCallback& callback);

  // Unregister a region that was previously registered by a call to
  // RegisterRegion. Any attempt to unregister a region that has not been
  // registered, or for which the registration is still in progress
  // (RegisterRegion hasn't called its callback yet) will fail.
  // TODO(mek): Maybe better behavior would be to allow unregistering still
  //     in-progress registrations.
  void UnregisterRegion(int64_t service_worker_registration_id,
                        const std::string& region_id,
                        const StatusCallback& callback);

  // Returns all currently registered regions. In case of failure (no geofencing
  // provider available for example) return an error status, while leaving
  // |regions| untouched.
  // This only returns regions for which the callback passed to RegisterRegion
  // has been called already (so it doesn't include still in progress
  // registrations).
  GeofencingStatus GetRegisteredRegions(
      int64_t service_worker_registration_id,
      std::map<std::string, blink::WebCircularGeofencingRegion>* result);

  // Enables or disables mock geofencing service.
  void SetMockProvider(GeofencingMockState mock_state);

  // Set the mock geofencing position.
  // TODO(mek): Unify this mock position with the devtools exposed geolocation
  // mock position (http://crbug.com/440902).
  void SetMockPosition(double latitude, double longitude);

  void SetServiceForTesting(GeofencingService* service) {
    service_ = service;
  }

 protected:
  friend class base::RefCountedThreadSafe<GeofencingManager>;
  ~GeofencingManager() override;

 private:
  // Internal bookkeeping associated with each registered geofence.
  struct Registration;

  void InitOnIO();
  void ShutdownOnIO();

  // ServiceWorkerContextObserver implementation.
  void OnRegistrationDeleted(int64_t service_worker_registration_id,
                             const GURL& pattern) override;

  // GeofencingRegistrationDelegate implementation.
  void RegistrationFinished(int64_t geofencing_registration_id,
                            GeofencingStatus status) override;
  void RegionEntered(int64_t geofencing_registration_id) override;
  void RegionExited(int64_t geofencing_registration_id) override;

  // Looks up a particular geofence registration. Returns nullptr if no
  // registration with the given IDs exists.
  Registration* FindRegistration(int64_t service_worker_registration_id,
                                 const std::string& region_id);

  // Looks up a particular geofence registration. Returns nullptr if no
  // registration with the given ID exists.
  Registration* FindRegistrationById(int64_t geofencing_registration_id);

  // Registers a new registration, returning a reference to the newly inserted
  // object. Assumes no registration with the same IDs currently exists.
  Registration& AddRegistration(
      int64_t service_worker_registration_id,
      const GURL& service_worker_origin,
      const std::string& region_id,
      const blink::WebCircularGeofencingRegion& region,
      const StatusCallback& callback,
      int64_t geofencing_registration_id);

  // Clears a registration.
  void ClearRegistration(Registration* registration);

  // Unregisters and clears all registrations associated with a specific
  // service worker.
  void CleanUpForServiceWorker(int64_t service_worker_registration_id);

  // Starts dispatching a particular geofencing |event_type| for the geofence
  // registration with the given ID. This first looks up the Service Worker
  // Registration the geofence is associated with, and then attempts to deliver
  // the event to that service worker.
  void DispatchGeofencingEvent(blink::WebGeofencingEventType event_type,
                               int64_t geofencing_registration_id);

  // Delivers an event to the specified service worker for the given geofence.
  // If the geofence registration id is no longer valid, this method does
  // nothing. This assumes the |service_worker_registration| is the service
  // worker the geofence registration is associated with.
  void DeliverGeofencingEvent(blink::WebGeofencingEventType event_type,
                              int64_t geofencing_registration_id,
                              ServiceWorkerStatusCode service_worker_status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  service_worker_registration);

  // Delivers an event to a specific service worker version. Called as callback
  // after the worker has started.
  void DeliverEventToRunningWorker(
      const scoped_refptr<ServiceWorkerRegistration>&
          service_worker_registration,
      blink::WebGeofencingEventType event_type,
      const std::string& region_id,
      const blink::WebCircularGeofencingRegion& region,
      const scoped_refptr<ServiceWorkerVersion>& worker);

  // Called for every response received for an event. There should only be one.
  void OnEventResponse(const scoped_refptr<ServiceWorkerVersion>& worker,
                       const scoped_refptr<ServiceWorkerRegistration>&
                           service_worker_registration,
                       int request_id,
                       blink::WebServiceWorkerEventResult result);

  // Called when dispatching an event failed.
  void OnEventError(ServiceWorkerStatusCode service_worker_status);

  // Map of all registered regions for a particular service worker registration.
  typedef std::map<std::string, Registration> RegionIdRegistrationMap;
  // Map of service worker registration id to the regions registered by that
  // service worker.
  typedef std::map<int64_t, RegionIdRegistrationMap>
      ServiceWorkerRegistrationsMap;
  ServiceWorkerRegistrationsMap registrations_;

  // Map of all registered regions by geofencing_registration_id.
  typedef std::map<int64_t, RegionIdRegistrationMap::iterator>
      RegistrationIdRegistrationMap;
  RegistrationIdRegistrationMap registrations_by_id_;

  GeofencingService* service_;
  scoped_ptr<MockGeofencingService> mock_service_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  DISALLOW_COPY_AND_ASSIGN(GeofencingManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_GEOFENCING_MANAGER_H_
