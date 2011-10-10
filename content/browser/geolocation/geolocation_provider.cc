// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  base::Closure task;
  if (observers_.empty()) {
    DCHECK(IsRunning());
    task = base::Bind(&GeolocationProvider::StopProviders,
                      base::Unretained(this));
  } else {
    if (!IsRunning()) {
      Start();
      if (HasPermissionBeenGranted())
        InformProvidersPermissionGranted(most_recent_authorized_frame_);
    }

    // The high accuracy requirement may have changed.
    task = base::Bind(&GeolocationProvider::StartProviders,
                      base::Unretained(this),
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
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&GeolocationProvider::InformProvidersPermissionGranted,
                   base::Unretained(this), requesting_frame));
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
  client_loop_->PostTask(
      FROM_HERE,
      base::Bind(&GeolocationProvider::NotifyObservers,
                 base::Unretained(this), position));
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
