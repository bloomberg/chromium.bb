// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SENSORS_H_
#define CONTENT_PUBLIC_BROWSER_SENSORS_H_
#pragma once

// The sensors API will unify various types of sensor data into a set of
// channels, each of which provides change events and periodic updates.
//
// This version of the API is intended only to support the experimental screen
// rotation code and is not for general use.

namespace content {

// The side of the device which the user probably perceives as facing upward.
// This can be used to orient controls and content in a "natural" direction.
enum ScreenOrientation {
  SCREEN_ORIENTATION_TOP = 0,     // The screen is in its normal orientation.
  // The screen is upside-down (180-degree rotation).
  SCREEN_ORIENTATION_BOTTOM = 1,
  SCREEN_ORIENTATION_LEFT = 2,    // The right side of the screen is at the top.
  SCREEN_ORIENTATION_RIGHT = 3,   // The left side of the screen is at the top.
  SCREEN_ORIENTATION_FRONT = 4,   // The screen is laid flat, facing upward.
  SCREEN_ORIENTATION_BACK = 5     // The screen is laid flat, facing downward.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SENSORS_H_
