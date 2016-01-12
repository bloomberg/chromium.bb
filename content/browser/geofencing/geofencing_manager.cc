// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geofencing/geofencing_manager.h"

#include <algorithm>

#include "base/callback.h"
#include "content/browser/geofencing/geofencing_service.h"
#include "content/browser/geofencing/mock_geofencing_service.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/geofencing_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"
#include "url/gurl.h"

namespace content {

struct GeofencingManager::Registration {
  Registration(int64_t service_worker_registration_id,
               const GURL& service_worker_origin,
               const std::string& region_id,
               const blink::WebCircularGeofencingRegion& region,
               const StatusCallback& callback,
               int64_t geofencing_registration_id);
  int64_t service_worker_registration_id;
  GURL service_worker_origin;
  std::string region_id;
  blink::WebCircularGeofencingRegion region;

  // Registration ID as returned by the |GeofencingService|.
  int64_t geofencing_registration_id;

  // Callback to call when registration is completed. This field is reset when
  // registration is complete.
  StatusCallback registration_callback;

  // Returns true if registration has been completed, and thus should be
  // included in calls to GetRegisteredRegions.
  bool is_active() const { return registration_callback.is_null(); }
};

GeofencingManager::Registration::Registration(
    int64_t service_worker_registration_id,
    const GURL& service_worker_origin,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region,
    const GeofencingManager::StatusCallback& callback,
    int64_t geofencing_registration_id)
    : service_worker_registration_id(service_worker_registration_id),
      service_worker_origin(service_worker_origin),
      region_id(region_id),
      region(region),
      geofencing_registration_id(geofencing_registration_id),
      registration_callback(callback) {}

GeofencingManager::GeofencingManager(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_(nullptr), service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GeofencingManager::~GeofencingManager() {
}

void GeofencingManager::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GeofencingManager::InitOnIO, this));
}

void GeofencingManager::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GeofencingManager::ShutdownOnIO, this));
}

void GeofencingManager::InitOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->AddObserver(this);
  if (!service_)
    service_ = GeofencingServiceImpl::GetInstance();
}

void GeofencingManager::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->RemoveObserver(this);
  // Clean up all registrations with the |GeofencingService|.
  // TODO(mek): This will need to change to support geofence registrations that
  //     outlive the browser, although removing the references to this
  //     |GeofencingManager| from the |GeofencingService| will still be needed.
  for (const auto& registration : registrations_by_id_)
    service_->UnregisterRegion(registration.first);
}

void GeofencingManager::RegisterRegion(
    int64_t service_worker_registration_id,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(mek): Validate region_id and region.

  // Look up service worker.
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    callback.Run(GEOFENCING_STATUS_OPERATION_FAILED_NO_SERVICE_WORKER);
    return;
  }

  GURL service_worker_origin =
      service_worker_registration->pattern().GetOrigin();

  if (!service_->IsServiceAvailable()) {
    callback.Run(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE);
    return;
  }

  if (FindRegistration(service_worker_registration_id, region_id)) {
    // Already registered, return an error.
    // TODO(mek): Use a more specific error code.
    callback.Run(GEOFENCING_STATUS_ERROR);
    return;
  }

  AddRegistration(service_worker_registration_id, service_worker_origin,
                  region_id, region, callback,
                  service_->RegisterRegion(region, this));
}

void GeofencingManager::UnregisterRegion(int64_t service_worker_registration_id,
                                         const std::string& region_id,
                                         const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(mek): Validate region_id.

  // Look up service worker.
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    callback.Run(GEOFENCING_STATUS_OPERATION_FAILED_NO_SERVICE_WORKER);
    return;
  }

  if (!service_->IsServiceAvailable()) {
    callback.Run(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE);
    return;
  }

  Registration* registration =
      FindRegistration(service_worker_registration_id, region_id);
  if (!registration) {
    // Not registered, return an error.
    callback.Run(GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED);
    return;
  }

  if (!registration->is_active()) {
    // Started registration, but not completed yet, error.
    callback.Run(GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED);
    return;
  }

  service_->UnregisterRegion(registration->geofencing_registration_id);
  ClearRegistration(registration);
  callback.Run(GEOFENCING_STATUS_OK);
}

GeofencingStatus GeofencingManager::GetRegisteredRegions(
    int64_t service_worker_registration_id,
    std::map<std::string, blink::WebCircularGeofencingRegion>* result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(result);

  // Look up service worker.
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    return GEOFENCING_STATUS_OPERATION_FAILED_NO_SERVICE_WORKER;
  }

  if (!service_->IsServiceAvailable()) {
    return GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE;
  }

  // Populate result, filtering out inactive registrations.
  result->clear();
  ServiceWorkerRegistrationsMap::iterator registrations =
      registrations_.find(service_worker_registration_id);
  if (registrations == registrations_.end())
    return GEOFENCING_STATUS_OK;
  for (const auto& registration : registrations->second) {
    if (registration.second.is_active())
      (*result)[registration.first] = registration.second.region;
  }
  return GEOFENCING_STATUS_OK;
}

void GeofencingManager::SetMockProvider(GeofencingMockState mock_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(mek): It would be nice if enabling the mock geofencing service
  // wouldn't completely delete all existing registrations but instead just
  // somehow keep them around but inactive.
  // For now mocking is only used in layout tests, so just clearing all
  // registrations works good enough.
  for (const auto& registration : registrations_by_id_)
    service_->UnregisterRegion(registration.first);
  registrations_by_id_.clear();
  registrations_.clear();

  // Then set or reset the mock service for the service worker.
  if (mock_state == GeofencingMockState::NONE) {
    service_ = GeofencingServiceImpl::GetInstance();
    mock_service_.reset();
  } else {
    mock_service_.reset(new MockGeofencingService(
        mock_state != GeofencingMockState::SERVICE_UNAVAILABLE));
    service_ = mock_service_.get();
  }
}

void GeofencingManager::SetMockPosition(double latitude, double longitude) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!mock_service_)
    return;
  mock_service_->SetMockPosition(latitude, longitude);
}

void GeofencingManager::OnRegistrationDeleted(
    int64_t service_worker_registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CleanUpForServiceWorker(service_worker_registration_id);
}

void GeofencingManager::RegistrationFinished(int64_t geofencing_registration_id,
                                             GeofencingStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Registration* registration = FindRegistrationById(geofencing_registration_id);
  DCHECK(registration);
  DCHECK(!registration->is_active());
  registration->registration_callback.Run(status);
  registration->registration_callback.Reset();

  // If the registration wasn't succesful, remove it from our storage.
  if (status != GEOFENCING_STATUS_OK)
    ClearRegistration(registration);
}

void GeofencingManager::RegionEntered(int64_t geofencing_registration_id) {
  DispatchGeofencingEvent(blink::WebGeofencingEventTypeEnter,
                          geofencing_registration_id);
}

void GeofencingManager::RegionExited(int64_t geofencing_registration_id) {
  DispatchGeofencingEvent(blink::WebGeofencingEventTypeLeave,
                          geofencing_registration_id);
}

GeofencingManager::Registration* GeofencingManager::FindRegistration(
    int64_t service_worker_registration_id,
    const std::string& region_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistrationsMap::iterator registrations_iterator =
      registrations_.find(service_worker_registration_id);
  if (registrations_iterator == registrations_.end())
    return nullptr;
  RegionIdRegistrationMap::iterator registration =
      registrations_iterator->second.find(region_id);
  if (registration == registrations_iterator->second.end())
    return nullptr;
  return &registration->second;
}

GeofencingManager::Registration* GeofencingManager::FindRegistrationById(
    int64_t geofencing_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RegistrationIdRegistrationMap::iterator registration_iterator =
      registrations_by_id_.find(geofencing_registration_id);
  if (registration_iterator == registrations_by_id_.end())
    return nullptr;
  return &registration_iterator->second->second;
}

GeofencingManager::Registration& GeofencingManager::AddRegistration(
    int64_t service_worker_registration_id,
    const GURL& service_worker_origin,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region,
    const StatusCallback& callback,
    int64_t geofencing_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!FindRegistration(service_worker_registration_id, region_id));
  RegionIdRegistrationMap::iterator registration =
      registrations_[service_worker_registration_id]
          .insert(std::make_pair(region_id,
                                 Registration(service_worker_registration_id,
                                              service_worker_origin,
                                              region_id,
                                              region,
                                              callback,
                                              geofencing_registration_id)))
          .first;
  registrations_by_id_[geofencing_registration_id] = registration;
  return registration->second;
}

void GeofencingManager::ClearRegistration(Registration* registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  registrations_by_id_.erase(registration->geofencing_registration_id);
  ServiceWorkerRegistrationsMap::iterator registrations_iterator =
      registrations_.find(registration->service_worker_registration_id);
  DCHECK(registrations_iterator != registrations_.end());
  registrations_iterator->second.erase(registration->region_id);
  if (registrations_iterator->second.empty())
    registrations_.erase(registrations_iterator);
}

void GeofencingManager::CleanUpForServiceWorker(
    int64_t service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistrationsMap::iterator registrations_iterator =
      registrations_.find(service_worker_registration_id);
  if (registrations_iterator == registrations_.end())
    return;

  for (const auto& registration : registrations_iterator->second) {
    int geofencing_registration_id =
        registration.second.geofencing_registration_id;
    service_->UnregisterRegion(geofencing_registration_id);
    registrations_by_id_.erase(geofencing_registration_id);
  }
  registrations_.erase(service_worker_registration_id);
}

void GeofencingManager::DispatchGeofencingEvent(
    blink::WebGeofencingEventType event_type,
    int64_t geofencing_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Registration* registration = FindRegistrationById(geofencing_registration_id);
  if (!registration ||
      registration->service_worker_registration_id ==
          kInvalidServiceWorkerRegistrationId) {
    // TODO(mek): Log/track these failures.
    return;
  }

  service_worker_context_->FindReadyRegistrationForId(
      registration->service_worker_registration_id,
      registration->service_worker_origin,
      base::Bind(&GeofencingManager::DeliverGeofencingEvent,
                 this,
                 event_type,
                 geofencing_registration_id));
}

void GeofencingManager::DeliverGeofencingEvent(
    blink::WebGeofencingEventType event_type,
    int64_t geofencing_registration_id,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Registration* registration = FindRegistrationById(geofencing_registration_id);
  if (!registration) {
    // Geofence got unregistered in the meantime, no longer need to deliver
    // event.
    return;
  }

  if (service_worker_status != SERVICE_WORKER_OK) {
    // TODO(mek): SW no longer exists, somehow handle this.
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  DCHECK(active_version);

  // Hold on to the service worker registration in the callback to keep it alive
  // until the callback dies. Otherwise the registration could be released when
  // this method returns - before the event is delivered to the service worker.
  active_version->RunAfterStartWorker(
      base::Bind(&GeofencingManager::OnEventError, this),
      base::Bind(&GeofencingManager::DeliverEventToRunningWorker, this,
                 service_worker_registration, event_type,
                 registration->region_id, registration->region,
                 make_scoped_refptr(active_version)));
}

void GeofencingManager::DeliverEventToRunningWorker(
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    blink::WebGeofencingEventType event_type,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region,
    const scoped_refptr<ServiceWorkerVersion>& worker) {
  int request_id =
      worker->StartRequest(ServiceWorkerMetrics::EventType::GEOFENCING,
                           base::Bind(&GeofencingManager::OnEventError, this));

  worker->DispatchEvent<ServiceWorkerHostMsg_GeofencingEventFinished>(
      request_id, ServiceWorkerMsg_GeofencingEvent(request_id, event_type,
                                                   region_id, region),
      base::Bind(&GeofencingManager::OnEventResponse, this, worker,
                 service_worker_registration));
}

void GeofencingManager::OnEventResponse(
    const scoped_refptr<ServiceWorkerVersion>& worker,
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  bool finish_result = worker->FinishRequest(request_id);
  DCHECK(finish_result)
      << "No messages should be passed to handler if request had "
         "already finished";
  // TODO(mek): log/check result.
}

void GeofencingManager::OnEventError(
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mek): log/check errors.
}

}  // namespace content
