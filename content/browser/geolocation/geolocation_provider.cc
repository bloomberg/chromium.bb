// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

GeolocationProvider* GeolocationProvider::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<GeolocationProvider>::get();
}

GeolocationProvider::GeolocationProvider()
    : base::Thread("Geolocation"),
      is_permission_granted_(false),
      ignore_location_updates_(false),
      arbitrator_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

GeolocationProvider::~GeolocationProvider() {
  // All observers should have unregistered before this singleton is destructed.
  DCHECK(observers_.empty());
  Stop();
  DCHECK(!arbitrator_);
}

void GeolocationProvider::AddObserver(GeolocationObserver* observer,
    const GeolocationObserverOptions& update_options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_[observer] = update_options;
  OnClientsChanged();
  if (position_.Validate() ||
      position_.error_code != content::Geoposition::ERROR_CODE_NONE)
    observer->OnLocationUpdate(position_);
}

bool GeolocationProvider::RemoveObserver(GeolocationObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  size_t removed = observers_.erase(observer);
  if (removed)
    OnClientsChanged();
  return removed > 0;
}

void GeolocationProvider::RequestCallback(
    const content::GeolocationUpdateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callbacks_.push_back(callback);
  OnClientsChanged();
  OnPermissionGranted();
}

void GeolocationProvider::OnClientsChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::Closure task;
  if (observers_.empty() && callbacks_.empty()) {
    DCHECK(IsRunning());
    task = base::Bind(&GeolocationProvider::StopProviders,
                      base::Unretained(this));
  } else {
    if (!IsRunning()) {
      Start();
      if (HasPermissionBeenGranted())
        InformProvidersPermissionGranted();
    }
    // Determine a set of options that satisfies all clients.
    GeolocationObserverOptions options =
        GeolocationObserverOptions::Collapse(observers_);
    // For callbacks, high accuracy position information is always requested.
    if (!callbacks_.empty())
      options.Collapse(GeolocationObserverOptions(true));

    // Send the current options to the providers as they may have changed.
    task = base::Bind(&GeolocationProvider::StartProviders,
                      base::Unretained(this),
                      options);
  }

  message_loop()->PostTask(FROM_HERE, task);
}

void GeolocationProvider::NotifyClients(const content::Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(position.Validate() ||
         position.error_code != content::Geoposition::ERROR_CODE_NONE);
  position_ = position;
  ObserverMap::const_iterator it = observers_.begin();
  while (it != observers_.end()) {
    // Advance iterator before calling the observer to guard against synchronous
    // unregister.
    GeolocationObserver* observer = it->first;
    ++it;
    observer->OnLocationUpdate(position_);
  }
  if (!callbacks_.empty()) {
    // Copy the callback list to guard against synchronous callback requests
    // reallocating the vector and invalidating the iterator.
    CallbackList callbacks = callbacks_;
    callbacks_.clear();
    for (CallbackList::iterator it = callbacks.begin();
         it != callbacks.end();
         ++it)
      it->Run(position);
    OnClientsChanged();
  }
}

void GeolocationProvider::StartProviders(
    const GeolocationObserverOptions& options) {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StartProviders(options);
}

void GeolocationProvider::StopProviders() {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StopProviders();
}

void GeolocationProvider::OnPermissionGranted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool was_permission_granted = is_permission_granted_;
  is_permission_granted_ = true;
  if (IsRunning() && !was_permission_granted)
    InformProvidersPermissionGranted();
}

void GeolocationProvider::InformProvidersPermissionGranted() {
  DCHECK(IsRunning());
  if (!OnGeolocationThread()) {
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&GeolocationProvider::InformProvidersPermissionGranted,
                   base::Unretained(this)));
    return;
  }
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->OnPermissionGranted();
}

void GeolocationProvider::Init() {
  DCHECK(OnGeolocationThread());
  DCHECK(!arbitrator_);
  arbitrator_ = GeolocationArbitrator::Create(this);
}

void GeolocationProvider::CleanUp() {
  DCHECK(OnGeolocationThread());
  delete arbitrator_;
  arbitrator_ = NULL;
}

void GeolocationProvider::OnLocationUpdate(
    const content::Geoposition& position) {
  DCHECK(OnGeolocationThread());
  // Will be true only in testing.
  if (ignore_location_updates_)
    return;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GeolocationProvider::NotifyClients,
                                     base::Unretained(this), position));
}

void GeolocationProvider::OverrideLocationForTesting(
    const content::Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  position_ = position;
  ignore_location_updates_ = true;
  NotifyClients(position);
}

bool GeolocationProvider::HasPermissionBeenGranted() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return is_permission_granted_;
}

bool GeolocationProvider::OnGeolocationThread() const {
  return MessageLoop::current() == message_loop();
}
