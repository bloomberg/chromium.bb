// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares the Position structure, which is used to represent a
// position fix. Originally derived from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation.h

#ifndef CHROME_BROWSER_GEOLOCATION_GEOPOSITION_H_
#define CHROME_BROWSER_GEOLOCATION_GEOPOSITION_H_

#include "base/string16.h"

// The internal representation of a position. Some properties use different
// types when passed to JavaScript.
struct Position {
 public:
  // Error codes for returning to JavaScript. These values are defined by the
  // W3C spec. Note that Gears does not use all of these codes, but we need
  // values for all of them to allow us to provide the constants on the error
  // object.
  enum ErrorCode {
    ERROR_CODE_NONE = -1,              // Gears addition
    ERROR_CODE_UNKNOWN_ERROR = 0,      // Not used by Gears
    ERROR_CODE_PERMISSION_DENIED = 1,  // Not used by Gears - Geolocation
                                       // methods throw an exception if
                                       // permission has not been granted.
    ERROR_CODE_POSITION_UNAVAILABLE = 2,
    ERROR_CODE_TIMEOUT = 3,
  };

  Position();

  bool is_valid_latlong() const;
  bool is_valid_altitude() const;
  bool is_valid_accuracy() const;
  bool is_valid_altitude_accuracy() const;
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
  int64 timestamp;           // Milliseconds since 1st Jan 1970

  // These properties are returned to JavaScript as a PositionError object.
  ErrorCode error_code;
  std::wstring error_message;  // Human-readable error message
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOPOSITION_H_
