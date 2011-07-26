// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SENSORS_LISTENER_H_
#define CONTENT_COMMON_SENSORS_LISTENER_H_
#pragma once

#include "content/common/sensors.h"

// The sensors API will unify various types of sensor data into a set of
// channels, each of which provides change events and periodic updates.
//
// This version of the API is intended only to support the experimental screen
// rotation code and is not for general use. In particular, the final listener
// will declare generic |OnSensorChanged| and |OnSensorUpdated| methods, rather
// than the special-purpose |OnScreenOrientationChanged|.

namespace sensors {

class Listener {
 public:
  // Called whenever the coarse orientation of the device changes.
  virtual void OnScreenOrientationChanged(const ScreenOrientation& change) = 0;

 protected:
  virtual ~Listener() {}
};

}  // namespace sensors

#endif  // CONTENT_COMMON_SENSORS_LISTENER_H_
