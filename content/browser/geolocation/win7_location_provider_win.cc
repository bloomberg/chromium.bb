// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/win7_location_provider_win.h"

#include <algorithm>
#include <cmath>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"

namespace{
const int kPollPeriodMovingMillis = 500;
// Poll less frequently whilst stationary.
const int kPollPeriodStationaryMillis = kPollPeriodMovingMillis * 3;
// Reading must differ by more than this amount to be considered movement.
const int kMovementThresholdMeters = 20;

// This algorithm is reused from the corresponding code in the gears project
// and is also used in gps_location_provider_linux.cc
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
}

Win7LocationProvider::Win7LocationProvider(Win7LocationApi* api)
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  DCHECK(api != NULL);
  api_.reset(api);
}

Win7LocationProvider::~Win7LocationProvider() {
  api_.reset();
}

bool Win7LocationProvider::StartProvider(bool high_accuracy){
  if (api_ == NULL)
    return false;
  api_->SetHighAccuracy(high_accuracy);
  if (task_factory_.empty())
    ScheduleNextPoll(0);
  return true;
}

void Win7LocationProvider::StopProvider() {
  task_factory_.RevokeAll();
}

void Win7LocationProvider::GetPosition(Geoposition* position) {
  DCHECK(position);
  *position = position_;
}

void Win7LocationProvider::UpdatePosition() {
  ScheduleNextPoll(0);
}

void Win7LocationProvider::OnPermissionGranted(
  const GURL& requesting_frame) {
}

void Win7LocationProvider::DoPollTask() {
  Geoposition new_position;
  api_->GetPosition(&new_position);
  const bool differ = PositionsDifferSiginificantly(position_, new_position);
  ScheduleNextPoll(differ ? kPollPeriodMovingMillis :
                            kPollPeriodStationaryMillis);
  if (differ || new_position.error_code != Geoposition::ERROR_CODE_NONE) {
    // Update if the new location is interesting or we have an error to report
    position_ = new_position;
    UpdateListeners();
  }
}

void Win7LocationProvider::ScheduleNextPoll(int interval) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(&Win7LocationProvider::DoPollTask),
      interval);
}

LocationProviderBase* NewSystemLocationProvider() {
  Win7LocationApi* api = Win7LocationApi::Create();
  if (api == NULL)
    return NULL; // API not supported on this machine.
  return new Win7LocationProvider(api);
}
