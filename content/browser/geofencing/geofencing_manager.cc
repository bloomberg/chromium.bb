// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geofencing/geofencing_manager.h"

#include <algorithm>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "content/browser/geofencing/geofencing_provider.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"
#include "url/gurl.h"

namespace content {

struct GeofencingManager::RegistrationKey {
  RegistrationKey(BrowserContext* browser_context,
                  int64 service_worker_registration_id,
                  const GURL& service_worker_origin,
                  const std::string& region_id);

  BrowserContext* browser_context;

  int64 service_worker_registration_id;
  GURL service_worker_origin;

  std::string region_id;
};

GeofencingManager::RegistrationKey::RegistrationKey(
    BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& service_worker_origin,
    const std::string& region_id)
    : browser_context(browser_context),
      service_worker_registration_id(service_worker_registration_id),
      service_worker_origin(service_worker_origin),
      region_id(region_id) {
}

struct GeofencingManager::Registration {
  Registration();
  Registration(const RegistrationKey& key,
               const blink::WebCircularGeofencingRegion& region);

  RegistrationKey key;
  blink::WebCircularGeofencingRegion region;

  // Registration id as returned by the GeofencingProvider, set to -1 if not
  // currently registered with the provider.
  int registration_id;

  // Flag to indicate if this registration has completed, and thus should be
  // included in calls to GetRegisteredRegions.
  bool is_active;
};

GeofencingManager::Registration::Registration() : key(nullptr, -1, GURL(), "") {
}

GeofencingManager::Registration::Registration(
    const RegistrationKey& key,
    const blink::WebCircularGeofencingRegion& region)
    : key(key), region(region), registration_id(-1), is_active(false) {
}

class GeofencingManager::RegistrationMatches {
 public:
  RegistrationMatches(const RegistrationKey& key) : key_(key) {}

  bool operator()(const Registration& registration) {
    return registration.key.browser_context == key_.browser_context &&
           registration.key.service_worker_registration_id ==
               key_.service_worker_registration_id &&
           registration.key.service_worker_origin ==
               key_.service_worker_origin &&
           registration.key.region_id == key_.region_id;
  }

 private:
  const RegistrationKey& key_;
};

GeofencingManager::GeofencingManager() {
}

GeofencingManager::~GeofencingManager() {
}

GeofencingManager* GeofencingManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return Singleton<GeofencingManager>::get();
}

void GeofencingManager::RegisterRegion(
    BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& service_worker_origin,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(mek): Validate region_id and region.

  if (!provider_.get()) {
    callback.Run(GeofencingStatus::
                     GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE);
    return;
  }

  RegistrationKey key(browser_context,
                      service_worker_registration_id,
                      service_worker_origin,
                      region_id);
  if (FindRegistration(key)) {
    // Already registered, return an error.
    callback.Run(GeofencingStatus::GEOFENCING_STATUS_ERROR);
    return;
  }

  // Add registration, but don't mark it as active yet. This prevents duplicate
  // registrations.
  AddRegistration(key, region);

  // Register with provider.
  provider_->RegisterRegion(
      region,
      base::Bind(&GeofencingManager::RegisterRegionCompleted,
                 base::Unretained(this),
                 callback,
                 key));
}

void GeofencingManager::UnregisterRegion(BrowserContext* browser_context,
                                         int64 service_worker_registration_id,
                                         const GURL& service_worker_origin,
                                         const std::string& region_id,
                                         const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(mek): Validate region_id.

  if (!provider_.get()) {
    callback.Run(GeofencingStatus::
                     GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE);
    return;
  }

  RegistrationKey key(browser_context,
                      service_worker_registration_id,
                      service_worker_origin,
                      region_id);
  Registration* registration = FindRegistration(key);
  if (!registration) {
    // Not registered, return an error/
    callback.Run(GeofencingStatus::GEOFENCING_STATUS_ERROR);
    return;
  }

  if (!registration->is_active) {
    // Started registration, but not completed yet, error.
    callback.Run(GeofencingStatus::GEOFENCING_STATUS_ERROR);
    return;
  }

  if (registration->registration_id != -1) {
    provider_->UnregisterRegion(registration->registration_id);
  }
  ClearRegistration(key);
  callback.Run(GeofencingStatus::GEOFENCING_STATUS_OK);
}

GeofencingStatus GeofencingManager::GetRegisteredRegions(
    BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& service_worker_origin,
    std::map<std::string, blink::WebCircularGeofencingRegion>* result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(result);

  if (!provider_.get()) {
    return GeofencingStatus::
        GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE;
  }

  // Populate result, filtering out inactive registrations.
  result->clear();
  for (const auto& registration : registrations_) {
    if (registration.key.browser_context == browser_context &&
        registration.key.service_worker_registration_id ==
            service_worker_registration_id &&
        registration.key.service_worker_origin == service_worker_origin &&
        registration.is_active) {
      (*result)[registration.key.region_id] = registration.region;
    }
  }
  return GeofencingStatus::GEOFENCING_STATUS_OK;
}

void GeofencingManager::RegisterRegionCompleted(const StatusCallback& callback,
                                                const RegistrationKey& key,
                                                GeofencingStatus status,
                                                int registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != GEOFENCING_STATUS_OK) {
    ClearRegistration(key);
    callback.Run(status);
    return;
  }

  Registration* registration = FindRegistration(key);
  DCHECK(registration);
  registration->registration_id = registration_id;
  registration->is_active = true;
  callback.Run(GeofencingStatus::GEOFENCING_STATUS_OK);
}

void GeofencingManager::SetProviderForTests(
    scoped_ptr<GeofencingProvider> provider) {
  DCHECK(!provider_.get());
  provider_ = provider.Pass();
}

GeofencingManager::Registration* GeofencingManager::FindRegistration(
    const RegistrationKey& key) {
  std::vector<Registration>::iterator it = std::find_if(
      registrations_.begin(), registrations_.end(), RegistrationMatches(key));
  if (it == registrations_.end())
    return nullptr;
  return &*it;
}

GeofencingManager::Registration& GeofencingManager::AddRegistration(
    const RegistrationKey& key,
    const blink::WebCircularGeofencingRegion& region) {
  DCHECK(!FindRegistration(key));
  registrations_.push_back(Registration(key, region));
  return registrations_.back();
}

void GeofencingManager::ClearRegistration(const RegistrationKey& key) {
  std::vector<Registration>::iterator it = std::find_if(
      registrations_.begin(), registrations_.end(), RegistrationMatches(key));
  if (it == registrations_.end())
    return;
  registrations_.erase(it);
}

}  // namespace content
