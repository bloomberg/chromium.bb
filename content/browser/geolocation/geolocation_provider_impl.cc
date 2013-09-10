// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_provider_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/geolocation/location_arbitrator_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
void OverrideLocationForTestingOnIOThread(
    const Geoposition& position,
    const base::Closure& completion_callback,
    scoped_refptr<base::MessageLoopProxy> callback_loop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationProviderImpl::GetInstance()->OverrideLocationForTesting(position);
  callback_loop->PostTask(FROM_HERE, completion_callback);
}
}  // namespace

GeolocationProvider* GeolocationProvider::GetInstance() {
  return GeolocationProviderImpl::GetInstance();
}

void GeolocationProvider::OverrideLocationForTesting(
    const Geoposition& position,
    const base::Closure& completion_callback) {
  base::Closure closure = base::Bind(&OverrideLocationForTestingOnIOThread,
                                     position,
                                     completion_callback,
                                     base::MessageLoopProxy::current());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, closure);
  else
    closure.Run();
}

void GeolocationProviderImpl::AddLocationUpdateCallback(
    const LocationUpdateCallback& callback, bool use_high_accuracy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool found = false;
  CallbackList::iterator i = callbacks_.begin();  
  for (; i != callbacks_.end(); ++i) {
    if (i->first.Equals(callback)) {
      i->second = use_high_accuracy;
      found = true;
      break;
    }
  }
  if (!found)
    callbacks_.push_back(std::make_pair(callback, use_high_accuracy));

  OnClientsChanged();
  if (position_.Validate() ||
      position_.error_code != Geoposition::ERROR_CODE_NONE) {
    callback.Run(position_);
  }
}

bool GeolocationProviderImpl::RemoveLocationUpdateCallback(
    const LocationUpdateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool removed = false;
  CallbackList::iterator i = callbacks_.begin();
  for (; i != callbacks_.end(); ++i) {
    if (i->first.Equals(callback)) {
      callbacks_.erase(i);
      removed = true;
      break;
    }
  }
  if (removed)
    OnClientsChanged();
  return removed;
}

void GeolocationProviderImpl::UserDidOptIntoLocationServices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool was_permission_granted = user_did_opt_into_location_services_;
  user_did_opt_into_location_services_ = true;
  if (IsRunning() && !was_permission_granted)
    InformProvidersPermissionGranted();
}

bool GeolocationProviderImpl::LocationServicesOptedIn() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return user_did_opt_into_location_services_;
}

void GeolocationProviderImpl::OnLocationUpdate(const Geoposition& position) {
  DCHECK(OnGeolocationThread());
  // Will be true only in testing.
  if (ignore_location_updates_)
    return;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GeolocationProviderImpl::NotifyClients,
                                     base::Unretained(this), position));
}

void GeolocationProviderImpl::OverrideLocationForTesting(
    const Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  position_ = position;
  ignore_location_updates_ = true;
  NotifyClients(position);
}

GeolocationProviderImpl* GeolocationProviderImpl::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<GeolocationProviderImpl>::get();
}

GeolocationProviderImpl::GeolocationProviderImpl()
    : base::Thread("Geolocation"),
      user_did_opt_into_location_services_(false),
      ignore_location_updates_(false),
      arbitrator_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

GeolocationProviderImpl::~GeolocationProviderImpl() {
  // All callbacks should have unregistered before this singleton is destructed.
  DCHECK(callbacks_.empty());
  Stop();
  DCHECK(!arbitrator_);
}

bool GeolocationProviderImpl::OnGeolocationThread() const {
  return base::MessageLoop::current() == message_loop();
}

void GeolocationProviderImpl::OnClientsChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::Closure task;
  if (callbacks_.empty()) {
    DCHECK(IsRunning());
    // We have no more observers, so we clear the cached geoposition so that
    // when the next observer is added we will not provide a stale position.
    position_ = Geoposition();
    task = base::Bind(&GeolocationProviderImpl::StopProviders,
                      base::Unretained(this));
  } else {
    if (!IsRunning()) {
      Start();
      if (LocationServicesOptedIn())
        InformProvidersPermissionGranted();
    }
    // Determine a set of options that satisfies all clients.
    bool use_high_accuracy = false;
    CallbackList::iterator i = callbacks_.begin();
    for (; i != callbacks_.end(); ++i) {
      if (i->second) {
        use_high_accuracy = true;
        break;
      }
    }

    // Send the current options to the providers as they may have changed.
    task = base::Bind(&GeolocationProviderImpl::StartProviders,
                      base::Unretained(this),
                      use_high_accuracy);
  }

  message_loop()->PostTask(FROM_HERE, task);
}

void GeolocationProviderImpl::StopProviders() {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StopProviders();
}

void GeolocationProviderImpl::StartProviders(bool use_high_accuracy) {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StartProviders(use_high_accuracy);
}

void GeolocationProviderImpl::InformProvidersPermissionGranted() {
  DCHECK(IsRunning());
  if (!OnGeolocationThread()) {
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&GeolocationProviderImpl::InformProvidersPermissionGranted,
                   base::Unretained(this)));
    return;
  }
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->OnPermissionGranted();
}

void GeolocationProviderImpl::NotifyClients(const Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(position.Validate() ||
         position.error_code != Geoposition::ERROR_CODE_NONE);
  position_ = position;
  CallbackList::const_iterator it = callbacks_.begin();
  while (it != callbacks_.end()) {
    // Advance iterator before calling the observer to guard against synchronous
    // unregister.
    LocationUpdateCallback callback = it->first;
    ++it;
    callback.Run(position_);
  }
}

void GeolocationProviderImpl::Init() {
  DCHECK(OnGeolocationThread());
  DCHECK(!arbitrator_);
  arbitrator_ = CreateArbitrator();
}

void GeolocationProviderImpl::CleanUp() {
  DCHECK(OnGeolocationThread());
  delete arbitrator_;
  arbitrator_ = NULL;
}

LocationArbitrator* GeolocationProviderImpl::CreateArbitrator() {
  LocationArbitratorImpl::LocationUpdateCallback callback = base::Bind(
      &GeolocationProviderImpl::OnLocationUpdate, base::Unretained(this));
  return new LocationArbitratorImpl(callback);
}

}  // namespace content
