// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_location_monitor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_provider.h"

namespace chromeos {

namespace {

std::string GeopositionToStringForDebug(const content::Geoposition& pos) {
  return base::StringPrintf(
      "latitude=%f, longitude=%f, altitude=%f, accuracy=%f, "
      "altitude_accuracy=%f, heading=%f, speed=%f",
      pos.latitude,
      pos.longitude,
      pos.altitude,
      pos.accuracy,
      pos.altitude_accuracy,
      pos.heading,
      pos.speed);
}

}  // namespace

LoginLocationMonitor::~LoginLocationMonitor() {
}

// static
LoginLocationMonitor* LoginLocationMonitor::GetInstance() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return Singleton<LoginLocationMonitor>::get();
}

// static
void LoginLocationMonitor::InstallLocationCallback(
    const base::TimeDelta timeout) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  LoginLocationMonitor* self = GetInstance();

  self->started_ = base::Time::Now();
  self->request_timeout_.Start(
      FROM_HERE,
      timeout,
      base::Bind(&LoginLocationMonitor::DoRemoveLocationCallback,
                 self->on_location_update_));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DoInstallLocationCallback, self->on_location_update_));
}

// static
void LoginLocationMonitor::RemoveLocationCallback() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  LoginLocationMonitor* self = GetInstance();
  self->request_timeout_.Stop();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DoRemoveLocationCallback, self->on_location_update_));
}

LoginLocationMonitor::LoginLocationMonitor()
    : weak_factory_(this),
      on_location_update_(
          base::Bind(&LoginLocationMonitor::OnLocationUpdatedIO)) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

// static
void LoginLocationMonitor::DoInstallLocationCallback(
    content::GeolocationProvider::LocationUpdateCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  content::GeolocationProvider* provider =
      content::GeolocationProvider::GetInstance();

  provider->AddLocationUpdateCallback(callback, false);
  provider->UserDidOptIntoLocationServices();
}

// static
void LoginLocationMonitor::DoRemoveLocationCallback(
    content::GeolocationProvider::LocationUpdateCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  content::GeolocationProvider* provider =
      content::GeolocationProvider::GetInstance();
  provider->RemoveLocationUpdateCallback(callback);
}

void LoginLocationMonitor::OnLocationUpdatedIO(
    const content::Geoposition& position) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!position.Validate()) {
    VLOG(1)
        << "LoginLocationMonitor::OnLocationUpdatedIO(): invalid position: {"
        << GeopositionToStringForDebug(position) << "}";
    return;
  }
  VLOG(1) << "LoginLocationMonitor::OnLocationUpdatedIO(): valid position: {"
          << GeopositionToStringForDebug(position) << "}";

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LoginLocationMonitor::OnLocationUpdatedUI, position));
}

void LoginLocationMonitor::OnLocationUpdatedUI(
    const content::Geoposition& position) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(position.Validate());

  LoginLocationMonitor* self = GetInstance();

  if (!self->request_timeout_.IsRunning()) {
    VLOG(1)
        << "LoginLocationMonitor::OnLocationUpdatedUI(): Service is stopped: {"
        << GeopositionToStringForDebug(position) << "}";
    return;
  }

  RemoveLocationCallback();

  // We need a cumulative timeout for timezone resolve, that is
  // (location resolve + timezone resolve). This is used to measure
  // timeout for the following timezone resolve.
  const base::TimeDelta elapsed = base::Time::Now() - self->started_;

  WizardController::OnLocationUpdated(position, elapsed);
}

}  // namespace chromeos
