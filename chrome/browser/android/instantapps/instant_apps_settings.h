// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_SETTINGS_H_
#define CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_SETTINGS_H_

#include <string>
#include "base/macros.h"

namespace content {
class WebContents;
}

// Instant Apps banner events are stored with other app banner events, but
// with an instant app specific key. This class should be used to store and
// retrieve information about the Instant App banner events.
class InstantAppsSettings {
 public:
  static void RecordInfoBarShowEvent(content::WebContents* web_contents,
                                     const std::string& url);
  static void RecordInfoBarDismissEvent(content::WebContents* web_contents,
                                        const std::string& url);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstantAppsSettings);
};

#endif  // CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_SETTINGS_H_
