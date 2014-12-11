// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "base/values.h"

// This class is needed to fetch the current location setting and provide it to
// the infobar.
class GoogleLocationSettingsHelper {
 public:
  virtual ~GoogleLocationSettingsHelper() {}

  static GoogleLocationSettingsHelper* Create();

  virtual bool IsLocationEnabled() { return IsSystemLocationEnabled(); }

  // Soon to be deprecated, please use IsLocationEnabled instead.
  virtual bool IsSystemLocationEnabled() { return false; }

 protected:
  GoogleLocationSettingsHelper() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleLocationSettingsHelper);
};

#endif  // CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_H_
