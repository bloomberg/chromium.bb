// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/gps_location_provider_linux.h"

#include <errno.h>

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "content/public/common/geoposition.h"

namespace content {
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
  const bool pos_1_valid = position_1.Validate();
  if (pos_1_valid != position_2.Validate())
    return true;
  if (!pos_1_valid) {
    DCHECK(!position_2.Validate());
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

#if defined(USE_LIBGPS)

// See http://crbug.com/103751.
COMPILE_ASSERT(GPSD_API_MAJOR_VERSION == 5, GPSD_API_version_is_not_5);

namespace {

const char kLibGpsName[] = "libgps.so.20";

}  // namespace

LibGps::LibGps()
    : gps_data_(new gps_data_t),
      is_open_(false) {
}

LibGps::~LibGps() {
  Stop();
}

LibGps* LibGps::New() {
  scoped_ptr<LibGps> libgps(new LibGps);
  if (!libgps->libgps_loader_.Load(kLibGpsName))
    return NULL;

  return libgps.release();
}

bool LibGps::Start() {
  if (is_open_)
    return true;

  errno = 0;
  if (libgps_loader_.gps_open(GPSD_SHARED_MEMORY, 0, gps_data_.get()) != 0) {
    // See gps.h NL_NOxxx for definition of gps_open() error numbers.
    DLOG(WARNING) << "gps_open() failed " << errno;
    return false;
  }

  is_open_ = true;
  return true;
}

void LibGps::Stop() {
  if (is_open_)
    libgps_loader_.gps_close(gps_data_.get());
  is_open_ = false;
}

bool LibGps::Read(Geoposition* position) {
  DCHECK(position);
  position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  if (!is_open_) {
      DLOG(WARNING) << "No gpsd connection";
      position->error_message = "No gpsd connection";
      return false;
  }

  if (libgps_loader_.gps_read(gps_data_.get()) < 0) {
      DLOG(WARNING) << "gps_read() fails";
      position->error_message = "gps_read() fails";
      return false;
  }

  if (!GetPositionIfFixed(position)) {
      DLOG(WARNING) << "No fixed position";
      position->error_message = "No fixed position";
      return false;
  }

  position->error_code = Geoposition::ERROR_CODE_NONE;
  position->timestamp = base::Time::Now();
  if (!position->Validate()) {
    // GetPositionIfFixed returned true, yet we've not got a valid fix.
    // This shouldn't happen; something went wrong in the conversion.
    NOTREACHED() << "Invalid position from GetPositionIfFixed: lat,long "
                 << position->latitude << "," << position->longitude
                 << " accuracy " << position->accuracy << " time "
                 << position->timestamp.ToDoubleT();
    position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = "Bad fix from gps";
    return false;
  }
  return true;
}

bool LibGps::GetPositionIfFixed(Geoposition* position) {
  DCHECK(position);
  if (gps_data_->status == STATUS_NO_FIX) {
    DVLOG(2) << "Status_NO_FIX";
    return false;
  }

  if (isnan(gps_data_->fix.latitude) || isnan(gps_data_->fix.longitude)) {
    DVLOG(2) << "No valid lat/lon value";
    return false;
  }

  position->latitude = gps_data_->fix.latitude;
  position->longitude = gps_data_->fix.longitude;

  if (!isnan(gps_data_->fix.epx) && !isnan(gps_data_->fix.epy)) {
    position->accuracy = std::max(gps_data_->fix.epx, gps_data_->fix.epy);
  } else if (isnan(gps_data_->fix.epx) && !isnan(gps_data_->fix.epy)) {
    position->accuracy = gps_data_->fix.epy;
  } else if (!isnan(gps_data_->fix.epx) && isnan(gps_data_->fix.epy)) {
    position->accuracy = gps_data_->fix.epx;
  } else {
    // TODO(joth): Fixme. This is a workaround for http://crbug.com/99326
    DVLOG(2) << "libgps reported accuracy NaN, forcing to zero";
    position->accuracy = 0;
  }

  if (gps_data_->fix.mode == MODE_3D && !isnan(gps_data_->fix.altitude)) {
    position->altitude = gps_data_->fix.altitude;
    if (!isnan(gps_data_->fix.epv))
      position->altitude_accuracy = gps_data_->fix.epv;
  }

  if (!isnan(gps_data_->fix.track))
    position->heading = gps_data_->fix.track;
  if (!isnan(gps_data_->fix.speed))
    position->speed = gps_data_->fix.speed;
  return true;
}

#else  // !defined(USE_LIBGPS)

// Stub implementation of LibGps.
LibGps::LibGps() {
}

LibGps::~LibGps() {
}

LibGps* LibGps::New() {
  return NULL;
}

bool LibGps::Start() {
  return false;
}

void LibGps::Stop() {
}

bool LibGps::Read(Geoposition* position) {
  return false;
}

bool LibGps::GetPositionIfFixed(Geoposition* position) {
  return false;
}

#endif  // !defined(USE_LIBGPS)

GpsLocationProviderLinux::GpsLocationProviderLinux(LibGpsFactory libgps_factory)
    : gpsd_reconnect_interval_millis_(kGpsdReconnectRetryIntervalMillis),
      poll_period_moving_millis_(kPollPeriodMovingMillis),
      poll_period_stationary_millis_(kPollPeriodStationaryMillis),
      libgps_factory_(libgps_factory),
      weak_factory_(this) {
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
  DCHECK(position->Validate() ||
         position->error_code != Geoposition::ERROR_CODE_NONE);
}

void GpsLocationProviderLinux::UpdatePosition() {
  ScheduleNextGpsPoll(0);
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

  DCHECK(new_position.Validate() ||
         new_position.error_code != Geoposition::ERROR_CODE_NONE);
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
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GpsLocationProviderLinux::DoGpsPollTask,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(interval));
}

LocationProviderBase* NewSystemLocationProvider() {
  return new GpsLocationProviderLinux(LibGps::New);
}

}  // namespace content
