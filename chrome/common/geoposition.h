// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares the Position structure, which is used to represent a
// position fix. Originally derived from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation.h

#ifndef CHROME_COMMON_GEOPOSITION_H_
#define CHROME_COMMON_GEOPOSITION_H_

#include <string>
#include "base/time.h"

// The internal representation of a geo position. Some properties use different
// types when passed to JavaScript.
struct Geoposition {
 public:
  // Error codes for returning to JavaScript. These values are defined by the
  // W3C spec. Note that Gears does not use all of these codes, but we need
  // values for all of them to allow us to provide the constants on the error
  // object.
  enum ErrorCode {
    ERROR_CODE_NONE = 0,               // Chrome addition
    ERROR_CODE_PERMISSION_DENIED = 1,
    ERROR_CODE_POSITION_UNAVAILABLE = 2,
    ERROR_CODE_TIMEOUT = 3,
  };

  Geoposition();

  bool is_valid_latlong() const;
  bool is_valid_altitude() const;
  bool is_valid_accuracy() const;
  bool is_valid_altitude_accuracy() const;
  bool is_valid_heading() const;
  bool is_valid_speed() const;
  bool is_valid_timestamp() const;

  // A valid fix has a valid latitude, longitude, accuracy and timestamp.
  bool IsValidFix() const;

  // A position is considered initialized if it has either a valid fix or
  // an error code other than NONE.
  bool IsInitialized() const;

  // These properties correspond to the JavaScript Position object.
  double latitude;           // In degrees
  double longitude;          // In degrees
  double altitude;           // In metres
  double accuracy;           // In metres
  double altitude_accuracy;  // In metres
  double heading;            // In degrees clockwise relative to the true north
  double speed;              // In meters per second
  // Timestamp for this position fix object taken from the host computer's
  // system clock (i.e. from Time::Now(), not the source device's clock).
  base::Time timestamp;

  // These properties are returned to JavaScript as a PositionError object.
  ErrorCode error_code;
  std::string error_message;   // Human-readable error message
};

#endif  // CHROME_COMMON_GEOPOSITION_H_
