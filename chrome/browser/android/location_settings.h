// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOCATION_SETTINGS_H_
#define CHROME_BROWSER_ANDROID_LOCATION_SETTINGS_H_

#include "base/macros.h"

namespace content {
class WebContents;
}

// This class determines whether Chrome can access the device's location,
// i.e. whether location is enabled system-wide on the device.
class LocationSettings {
 public:
  virtual ~LocationSettings() {}

  // Returns true if:
  //  - Location is enabled system-wide (in Android settings)
  //  - The necessary location permission are granted to Chrome, or if Chrome
  //    still has the ability to request the permissions to be granted.
  virtual bool CanSitesRequestLocationPermission(
      content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_ANDROID_LOCATION_SETTINGS_H_
