// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOCATION_SETTINGS_H_
#define CHROME_BROWSER_ANDROID_LOCATION_SETTINGS_H_

#include "base/macros.h"

// This class determines whether Chrome can access the device's location,
// i.e. whether location is enabled system-wide on the device.
class LocationSettings {
 public:
  virtual ~LocationSettings() {}

  // Returns true if location is enabled system-wide (in Android settings).
  virtual bool IsLocationEnabled() = 0;
};

#endif  // CHROME_BROWSER_ANDROID_LOCATION_SETTINGS_H_
