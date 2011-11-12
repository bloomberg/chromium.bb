// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/gps_location_provider_linux.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
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

// This algorithm is reused from the corresponding code in the Gears project.
// The arbitrary delta is decreased (Gears used 100 meters); if we need to
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
    : gpsd_reconnect_interval_millis_(kGpsdReconnectRetryIntervalMillis),
      poll_period_moving_millis_(kPollPeriodMovingMillis),
      poll_period_stationary_millis_(kPollPeriodStationaryMillis),
      libgps_factory_(libgps_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
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
    DCHECK(weak_factory_.HasWeakPtrs());
    return true;
  }
  position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  gps_.reset(libgps_factory_());
  if (gps_ == NULL) {
    DLOG(WARNING) << "libgps could not be loaded";
    return false;
  }
  ScheduleNextGpsPoll(0);
  return true;
}

void GpsLocationProviderLinux::StopProvider() {
  weak_factory_.InvalidateWeakPtrs();
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
    ScheduleNextGpsPoll(gpsd_reconnect_interval_millis_);
    return;
  }

  Geoposition new_position;
  if (!gps_->Read(&new_position)) {
    ScheduleNextGpsPoll(poll_period_stationary_millis_);
    return;
  }

  DCHECK(new_position.IsInitialized());
  const bool differ = PositionsDifferSiginificantly(position_, new_position);
  ScheduleNextGpsPoll(differ ? poll_period_moving_millis_ :
                               poll_period_stationary_millis_);
  if (differ || new_position.error_code != Geoposition::ERROR_CODE_NONE) {
    // Update if the new location is interesting or we have an error to report.
    position_ = new_position;
    UpdateListeners();
  }
}

void GpsLocationProviderLinux::ScheduleNextGpsPoll(int interval) {
  weak_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GpsLocationProviderLinux::DoGpsPollTask,
                 weak_factory_.GetWeakPtr()),
      interval);
}

LocationProviderBase* NewSystemLocationProvider() {
  return new GpsLocationProviderLinux(LibGps::New);
}
