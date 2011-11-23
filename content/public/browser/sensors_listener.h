// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SENSORS_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_SENSORS_LISTENER_H_
#pragma once

#include "content/public/browser/sensors.h"

// The sensors API will unify various types of sensor data into a set of
// channels, each of which provides change events and periodic updates.
//
// This version of the API is intended only to support the experimental screen
// rotation code and is not for general use. In particular, the final listener
// will declare generic |OnSensorChanged| and |OnSensorUpdated| methods, rather
// than the special-purpose |OnScreenOrientationChanged|.

namespace content {

class SensorsListener {
 public:
  // Called whenever the coarse orientation of the device changes.
  virtual void OnScreenOrientationChanged(ScreenOrientation change) = 0;

 protected:
  virtual ~SensorsListener() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SENSORS_LISTENER_H_
