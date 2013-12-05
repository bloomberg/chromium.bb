// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_

#include <string>

#include "extensions/common/constants.h"

namespace extensions {

class Extension;
class ExtensionPrefs;

// This enum is used for the launch type the user wants to use for an
// application.
// Do not remove items or re-order this enum as it is used in preferences
// and histograms.
enum LaunchType {
  LAUNCH_TYPE_PINNED,
  LAUNCH_TYPE_REGULAR,
  LAUNCH_TYPE_FULLSCREEN,
  LAUNCH_TYPE_WINDOW,

  // Launch an app in the in the way a click on the NTP would,
  // if no user pref were set.  Update this constant to change
  // the default for the NTP and chrome.management.launchApp().
  LAUNCH_TYPE_DEFAULT = LAUNCH_TYPE_REGULAR
};

// Gets the launch type preference. If no preference is set, returns
// LAUNCH_DEFAULT.
// Returns LAUNCH_WINDOW if there's no preference and
// 'streamlined hosted apps' are enabled.
LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                         const Extension* extension);

// Sets an extension's launch type preference.
void SetLaunchType(ExtensionPrefs* prefs,
                   const std::string& extension_id,
                   LaunchType launch_type);

// Finds the right launch container based on the launch type.
// If |extension|'s prefs do not have a launch type set, then the default
// value from GetLaunchType() is used to choose the launch container.
LaunchContainer GetLaunchContainer(const ExtensionPrefs* prefs,
                                   const Extension* extension);

// Returns true if a launch container preference has been specified for
// |extension|. GetLaunchContainer() will still return a default value even if
// this returns false.
bool HasPreferredLaunchContainer(const ExtensionPrefs* prefs,
                                 const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_
