// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geoposition.h"

namespace {
// Sentinel values to mark invalid data. (WebKit carries companion is_valid
// bools for this purpose; we may eventually follow that approach, but
// sentinels worked OK in the gears code this is based on.)
const double kBadLatLng = 200;
// Lowest point on land is at approximately -400 metres.
const int kBadAltitude = -1000;
const int kBadAccuracy = -1;  // Accuracy must be non-negative.
const int64 kBadTimestamp = kint64min;
}

Position::Position()
    : latitude(kBadLatLng),
      longitude(kBadLatLng),
      altitude(kBadAltitude),
      accuracy(kBadAccuracy),
      altitude_accuracy(kBadAccuracy),
      timestamp(kBadTimestamp),
      error_code(ERROR_CODE_NONE) {
}

bool Position::is_valid_latlong() const {
  return latitude >= -90.0 && latitude <= 90.0 &&
         longitude >= -180.0 && longitude <= 180.0;
}

bool Position::is_valid_altitude() const {
  return altitude > kBadAltitude;
}

bool Position::is_valid_accuracy() const {
  return accuracy >= 0.0;
}

bool Position::is_valid_altitude_accuracy() const {
  return altitude_accuracy >= 0.0;
}

bool Position::is_valid_timestamp() const {
  return timestamp != kBadTimestamp;
}

bool Position::IsValidFix() const {
  return is_valid_latlong() && is_valid_accuracy() && is_valid_timestamp();
}

bool Position::IsInitialized() const {
  return error_code != ERROR_CODE_NONE || IsValidFix();
}
