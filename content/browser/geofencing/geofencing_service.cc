// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geofencing/geofencing_service.h"

#include <utility>

#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/geofencing/geofencing_provider.h"
#include "content/browser/geofencing/geofencing_registration_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

namespace content {

namespace {

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

}  // namespace

struct GeofencingServiceImpl::Registration {
  Registration();
  Registration(const blink::WebCircularGeofencingRegion& region,
               int64_t geofencing_registration_id,
               GeofencingRegistrationDelegate* delegate);

  blink::WebCircularGeofencingRegion region;
  int64_t geofencing_registration_id;
  GeofencingRegistrationDelegate* delegate;

  enum RegistrationState {
    // In progress of being registered with provider.
    STATE_REGISTERING,
    // Currently registered with provider.
    STATE_REGISTERED,
    // In progress of being registered with provider, but should be unregistered
    // and deleted.
    STATE_SHOULD_UNREGISTER_AND_DELETE,
    // Not currently registered with provider, but still an active registration.
    STATE_UNREGISTERED
  };
  RegistrationState state;
};

GeofencingServiceImpl::Registration::Registration()
    : geofencing_registration_id(-1),
      delegate(nullptr),
      state(STATE_UNREGISTERED) {
}

GeofencingServiceImpl::Registration::Registration(
    const blink::WebCircularGeofencingRegion& region,
    int64_t geofencing_registration_id,
    GeofencingRegistrationDelegate* delegate)
    : region(region),
      geofencing_registration_id(geofencing_registration_id),
      delegate(delegate),
      state(STATE_REGISTERING) {}

GeofencingServiceImpl::GeofencingServiceImpl() : next_registration_id_(0) {
}

GeofencingServiceImpl::~GeofencingServiceImpl() {
}

GeofencingServiceImpl* GeofencingServiceImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::Singleton<GeofencingServiceImpl>::get();
}

bool GeofencingServiceImpl::IsServiceAvailable() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return EnsureProvider();
}

int64_t GeofencingServiceImpl::RegisterRegion(
    const blink::WebCircularGeofencingRegion& region,
    GeofencingRegistrationDelegate* delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  int64_t geofencing_registration_id = GetNextId();
  registrations_[geofencing_registration_id] =
      Registration(region, geofencing_registration_id, delegate);

  if (!EnsureProvider()) {
    RunSoon(
        base::Bind(&GeofencingServiceImpl::NotifyRegistrationFinished,
                   base::Unretained(this),
                   geofencing_registration_id,
                   GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE));
    return geofencing_registration_id;
  }

  provider_->RegisterRegion(
      geofencing_registration_id,
      region,
      base::Bind(&GeofencingServiceImpl::NotifyRegistrationFinished,
                 base::Unretained(this),
                 geofencing_registration_id));
  return geofencing_registration_id;
}

void GeofencingServiceImpl::UnregisterRegion(
    int64_t geofencing_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RegistrationsMap::iterator registration_iterator =
      registrations_.find(geofencing_registration_id);
  DCHECK(registration_iterator != registrations_.end());

  if (!EnsureProvider())
    return;

  switch (registration_iterator->second.state) {
    case Registration::STATE_REGISTERED:
      provider_->UnregisterRegion(geofencing_registration_id);
    // fallthru
    case Registration::STATE_UNREGISTERED:
      registrations_.erase(registration_iterator);
      break;
    case Registration::STATE_REGISTERING:
      // Update state, NotifyRegistrationFinished will take care of actually
      // unregistering.
      registration_iterator->second.state =
          Registration::STATE_SHOULD_UNREGISTER_AND_DELETE;
      break;
    case Registration::STATE_SHOULD_UNREGISTER_AND_DELETE:
      // Should not happen.
      NOTREACHED();
      break;
  }
}

void GeofencingServiceImpl::SetProviderForTesting(
    scoped_ptr<GeofencingProvider> provider) {
  DCHECK(!provider_.get());
  provider_ = std::move(provider);
}

int GeofencingServiceImpl::RegistrationCountForTesting() {
  return registrations_.size();
}

bool GeofencingServiceImpl::EnsureProvider() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!provider_) {
    // TODO(mek): Create platform specific provider.
    return false;
  }
  return true;
}

int64_t GeofencingServiceImpl::GetNextId() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return next_registration_id_++;
}

void GeofencingServiceImpl::NotifyRegistrationFinished(
    int64_t geofencing_registration_id,
    GeofencingStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RegistrationsMap::iterator registration_iterator =
      registrations_.find(geofencing_registration_id);
  DCHECK(registration_iterator != registrations_.end());
  DCHECK(registration_iterator->second.state ==
             Registration::STATE_REGISTERING ||
         registration_iterator->second.state ==
             Registration::STATE_SHOULD_UNREGISTER_AND_DELETE);

  if (registration_iterator->second.state ==
      Registration::STATE_SHOULD_UNREGISTER_AND_DELETE) {
    // Don't call delegate, but unregister with provider if registration was
    // succesfull.
    if (status == GEOFENCING_STATUS_OK) {
      provider_->UnregisterRegion(geofencing_registration_id);
    }
    registrations_.erase(registration_iterator);
    return;
  }

  // Normal case, mark as registered and call delegate.
  registration_iterator->second.state = Registration::STATE_REGISTERED;
  registration_iterator->second.delegate->RegistrationFinished(
      geofencing_registration_id, status);

  if (status != GEOFENCING_STATUS_OK) {
    // Registration failed, remove from our book-keeping.
    registrations_.erase(registration_iterator);
  }
}

}  // namespace content
