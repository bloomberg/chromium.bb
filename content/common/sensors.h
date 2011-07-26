// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SENSORS_H_
#define CONTENT_COMMON_SENSORS_H_
#pragma once

// The sensors API will unify various types of sensor data into a set of
// channels, each of which provides change events and periodic updates.
//
// This version of the API is intended only to support the experimental screen
// rotation code and is not for general use.

namespace sensors {

// Indicates the coarse orientation of the device.
struct ScreenOrientation {
  enum Side {
    TOP = 0,     // The screen is in its normal orientation.
    BOTTOM = 1,  // The screen is upside-down (180-degree rotation).
    LEFT = 2,    // The right side of the screen is at the top.
    RIGHT = 3,   // The left side of the screen is at the top.
    FRONT = 4,   // The screen is laid flat, facing upward.
    BACK = 5     // The screen is laid flat, facing downward.
  };

  // The side of the device which the user probably perceives as facing upward.
  // This can be used to orient controls and content in a "natural" direction.
  Side upward;
};

}  // namespace sensors

#endif  // CONTENT_COMMON_SENSORS_H_
