// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/gps_location_provider_linux.h"

#include <algorithm>
#include <cmath>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "content/browser/geolocation/libgps_wrapper_linux.h"

namespace {
const int kGpsdReconnectRetryIntervalMillis = 10 * 1000;
// As per http://gpsd.berlios.de/performance.html#id374524, poll twice per sec.
const int kPollPeriodMovingMillis = 500;
// Poll less frequently whilst stationary.
const int kPollPeriodStationaryMillis = kPollPeriodMovingMillis * 3;
// GPS reading must differ by more than this amount to be considered movement.
const int kMovementThresholdMeters = 20;

// This algorithm is reused from the corresponding code in the gears project.
// The arbitrary delta is decreased (gears used 100 meters); if we need to
// decrease it any further we'll likely want to do some smarter filtering to
// remove GPS location jitter noise.
bool PositionsDifferSiginificantly(const Geoposition& position_1,
                                   const Geoposition& position_2) {
  const bool pos_1_valid = position_1.IsValidFix();
  if (pos_1_valid != position_2.IsValidFix())
    return true;
  if (!pos_1_valid) {
    DCHECK(!position_2.IsValidFix());
    return false;
  }
  double delta = std::sqrt(
      std::pow(std::fabs(position_1.latitude - position_2.latitude), 2) +
      std::pow(std::fabs(position_1.longitude - position_2.longitude), 2));
  // Convert to meters. 1 minute of arc of latitude (or longitude at the
  // equator) is 1 nautical mile or 1852m.
  delta *= 60 * 1852;
  return delta > kMovementThresholdMeters;
}
}  // namespace

GpsLocationProviderLinux::GpsLocationProviderLinux(LibGpsFactory libgps_factory)
    : libgps_factory_(libgps_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  DCHECK(libgps_factory_);
}

GpsLocationProviderLinux::~GpsLocationProviderLinux() {
}

bool GpsLocationProviderLinux::StartProvider(bool high_accuracy) {
  if (!high_accuracy) {
    StopProvider();
    return true;  // Not an error condition, so still return true.
  }
  if (gps_ != NULL) {
    DCHECK(!task_factory_.empty());
    return true;
  }
  position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  gps_.reset(libgps_factory_());
  if (gps_ == NULL) {
    DLOG(WARNING) << "libgps.so could not be loaded";
    return false;
  }
  ScheduleNextGpsPoll(0);
  return true;
}

void GpsLocationProviderLinux::StopProvider() {
  task_factory_.RevokeAll();
  gps_.reset();
}

void GpsLocationProviderLinux::GetPosition(Geoposition* position) {
  DCHECK(position);
  *position = position_;
  DCHECK(position->IsInitialized());
}

void GpsLocationProviderLinux::UpdatePosition() {
  ScheduleNextGpsPoll(0);
}

void GpsLocationProviderLinux::OnPermissionGranted(
    const GURL& requesting_frame) {
}

void GpsLocationProviderLinux::DoGpsPollTask() {
  if (!gps_->Start()) {
    DLOG(WARNING) << "Couldn't start GPS provider.";
    ScheduleNextGpsPoll(kGpsdReconnectRetryIntervalMillis);
    return;
  }
  if (!gps_->Poll()) {
    ScheduleNextGpsPoll(kPollPeriodStationaryMillis);
    return;
  }
  Geoposition new_position;
  gps_->GetPosition(&new_position);
  DCHECK(new_position.IsInitialized());
  const bool differ = PositionsDifferSiginificantly(position_, new_position);
  ScheduleNextGpsPoll(differ ? kPollPeriodMovingMillis :
                               kPollPeriodStationaryMillis);
  if (differ || new_position.error_code != Geoposition::ERROR_CODE_NONE) {
    // Update if the new location is interesting or we have an error to report.
    position_ = new_position;
    UpdateListeners();
  }
}

void GpsLocationProviderLinux::ScheduleNextGpsPoll(int interval) {
  task_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(&GpsLocationProviderLinux::DoGpsPollTask),
      interval);
}

LocationProviderBase* NewSystemLocationProvider() {
  return new GpsLocationProviderLinux(LibGps::New);
}
