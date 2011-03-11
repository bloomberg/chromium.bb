// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/geoposition.h"

namespace {
// Sentinel values to mark invalid data. (WebKit carries companion is_valid
// bools for this purpose; we may eventually follow that approach, but
// sentinels worked OK in the Gears code this is based on.)
const double kBadLatitudeLongitude = 200;
// Lowest point on land is at approximately -400 meters.
const int kBadAltitude = -10000;
const int kBadAccuracy = -1;  // Accuracy must be non-negative.
const int64 kBadTimestamp = kint64min;
const int kBadHeading = -1;  // Heading must be non-negative.
const int kBadSpeed = -1;
}

Geoposition::Geoposition()
    : latitude(kBadLatitudeLongitude),
      longitude(kBadLatitudeLongitude),
      altitude(kBadAltitude),
      accuracy(kBadAccuracy),
      altitude_accuracy(kBadAccuracy),
      heading(kBadHeading),
      speed(kBadSpeed),
      error_code(ERROR_CODE_NONE) {
}

bool Geoposition::is_valid_latlong() const {
  return latitude >= -90.0 && latitude <= 90.0 &&
         longitude >= -180.0 && longitude <= 180.0;
}

bool Geoposition::is_valid_altitude() const {
  return altitude > kBadAltitude;
}

bool Geoposition::is_valid_accuracy() const {
  return accuracy >= 0.0;
}

bool Geoposition::is_valid_altitude_accuracy() const {
  return altitude_accuracy >= 0.0;
}

bool Geoposition::is_valid_heading() const {
  return heading >= 0 && heading <= 360;
}

bool Geoposition::is_valid_speed() const {
  return speed >= 0;
}

bool Geoposition::is_valid_timestamp() const {
  return !timestamp.is_null();
}

bool Geoposition::IsValidFix() const {
  return is_valid_latlong() && is_valid_accuracy() && is_valid_timestamp();
}

bool Geoposition::IsInitialized() const {
  return error_code != ERROR_CODE_NONE || IsValidFix();
}
