// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/geolocation/location_arbitrator.h"

GeolocationProvider* GeolocationProvider::GetInstance() {
  return Singleton<GeolocationProvider>::get();
}

GeolocationProvider::GeolocationProvider()
    : base::Thread("Geolocation"),
      client_loop_(base::MessageLoopProxy::current()),
      is_permission_granted_(false),
      ignore_location_updates_(false),
      arbitrator_(NULL) {
}

GeolocationProvider::~GeolocationProvider() {
  DCHECK(observers_.empty());  // observers must unregister.
  Stop();
  DCHECK(!arbitrator_);
}

void GeolocationProvider::AddObserver(GeolocationObserver* observer,
    const GeolocationObserverOptions& update_options) {
  DCHECK(OnClientThread());
  observers_[observer] = update_options;
  OnObserversChanged();
  if (position_.Validate() ||
      position_.error_code != content::Geoposition::ERROR_CODE_NONE)
    observer->OnLocationUpdate(position_);
}

bool GeolocationProvider::RemoveObserver(GeolocationObserver* observer) {
  DCHECK(OnClientThread());
  size_t remove = observers_.erase(observer);
  OnObserversChanged();
  return remove > 0;
}

void GeolocationProvider::OnObserversChanged() {
  DCHECK(OnClientThread());
  base::Closure task;
  if (observers_.empty()) {
    DCHECK(IsRunning());
    task = base::Bind(&GeolocationProvider::StopProviders,
                      base::Unretained(this));
  } else {
    if (!IsRunning()) {
      Start();
      if (HasPermissionBeenGranted())
        InformProvidersPermissionGranted();
    }

    // The high accuracy requirement may have changed.
    task = base::Bind(&GeolocationProvider::StartProviders,
                      base::Unretained(this),
                      GeolocationObserverOptions::Collapse(observers_));
  }

  message_loop()->PostTask(FROM_HERE, task);
}

void GeolocationProvider::NotifyObservers(
    const content::Geoposition& position) {
  DCHECK(OnClientThread());
  DCHECK(position.Validate() ||
         position.error_code != content::Geoposition::ERROR_CODE_NONE);
  position_ = position;
  ObserverMap::const_iterator it = observers_.begin();
  while (it != observers_.end()) {
    // Advance iterator before callback to guard against synchronous unregister.
    GeolocationObserver* observer = it->first;
    ++it;
    observer->OnLocationUpdate(position_);
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
  DCHECK(OnClientThread());
  is_permission_granted_ = true;
  if (IsRunning())
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
  client_loop_->PostTask(
      FROM_HERE,
      base::Bind(&GeolocationProvider::NotifyObservers,
                 base::Unretained(this), position));
}

void GeolocationProvider::OverrideLocationForTesting(
    const content::Geoposition& position) {
  DCHECK(OnClientThread());
  position_ = position;
  ignore_location_updates_ = true;
  NotifyObservers(position);
}

bool GeolocationProvider::HasPermissionBeenGranted() const {
  DCHECK(OnClientThread());
  return is_permission_granted_;
}

bool GeolocationProvider::OnClientThread() const {
  return client_loop_->BelongsToCurrentThread();
}

bool GeolocationProvider::OnGeolocationThread() const {
  return MessageLoop::current() == message_loop();
}
