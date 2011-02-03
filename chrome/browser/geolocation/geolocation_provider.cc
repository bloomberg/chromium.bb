// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_provider.h"

#include "base/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/geolocation/location_arbitrator.h"

// This class is guaranteed to outlive its internal thread, so ref counting
// is not required.
DISABLE_RUNNABLE_METHOD_REFCOUNT(GeolocationProvider);

GeolocationProvider* GeolocationProvider::GetInstance() {
  return Singleton<GeolocationProvider>::get();
}

GeolocationProvider::GeolocationProvider()
    : base::Thread("Geolocation"),
      client_loop_(base::MessageLoopProxy::CreateForCurrentThread()),
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
  if (position_.IsInitialized())
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
  Task* task = NULL;
  if (observers_.empty()) {
    DCHECK(IsRunning());
    task = NewRunnableMethod(this, &GeolocationProvider::StopProviders);
  } else {
    if (!IsRunning()) {
      Start();
      if (HasPermissionBeenGranted())
        InformProvidersPermissionGranted(most_recent_authorized_frame_);
    }
    // The high accuracy requirement may have changed.
    task = NewRunnableMethod(
        this,
        &GeolocationProvider::StartProviders,
        GeolocationObserverOptions::Collapse(observers_));
  }
  message_loop()->PostTask(FROM_HERE, task);
}

void GeolocationProvider::NotifyObservers(const Geoposition& position) {
  DCHECK(OnClientThread());
  DCHECK(position.IsInitialized());
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

void GeolocationProvider::OnPermissionGranted(const GURL& requesting_frame) {
  DCHECK(OnClientThread());
  most_recent_authorized_frame_ = requesting_frame;
  if (IsRunning())
    InformProvidersPermissionGranted(requesting_frame);
}

void GeolocationProvider::InformProvidersPermissionGranted(
    const GURL& requesting_frame) {
  DCHECK(IsRunning());
  DCHECK(requesting_frame.is_valid());
  if (!OnGeolocationThread()) {
    Task* task = NewRunnableMethod(
        this,
        &GeolocationProvider::InformProvidersPermissionGranted,
        requesting_frame);
    message_loop()->PostTask(FROM_HERE, task);
    return;
  }
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->OnPermissionGranted(requesting_frame);
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

void GeolocationProvider::OnLocationUpdate(const Geoposition& position) {
  DCHECK(OnGeolocationThread());
  Task* task = NewRunnableMethod(this,
                                 &GeolocationProvider::NotifyObservers,
                                 position);
  client_loop_->PostTask(FROM_HERE, task);
}

bool GeolocationProvider::HasPermissionBeenGranted() const {
  DCHECK(OnClientThread());
  return most_recent_authorized_frame_.is_valid();
}

bool GeolocationProvider::OnClientThread() const {
  return client_loop_->BelongsToCurrentThread();
}

bool GeolocationProvider::OnGeolocationThread() const {
  return MessageLoop::current() == message_loop();
}
