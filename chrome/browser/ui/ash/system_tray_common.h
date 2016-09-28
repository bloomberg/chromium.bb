// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_COMMON_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_COMMON_H_

#include <string>

#include "base/macros.h"

// System tray code shared between classic ash SystemTrayDelegateChromeos and
// mustash SystemTrayClient.
class SystemTrayCommon {
 public:
  static const char kDisplaySettingsSubPageName[];
  static const char kDisplayOverscanSettingsSubPageName[];

  // Shows general settings UI.
  static void ShowSettings();

  // Shows the settings related to date, timezone etc.
  static void ShowDateSettings();

  // Shows settings related to multiple displays.
  static void ShowDisplaySettings();

  // Shows the page that lets you disable performance tracing.
  static void ShowChromeSlow();

  // Shows settings related to input methods.
  static void ShowIMESettings();

  // Shows help.
  static void ShowHelp();

  // Show accessibility help.
  static void ShowAccessibilityHelp();

  // Show the settings related to accessibility.
  static void ShowAccessibilitySettings();

  // Shows the help center article for the stylus tool palette.
  static void ShowPaletteHelp();

  // Shows the settings related to the stylus tool palette.
  static void ShowPaletteSettings();

  // Shows information about public account mode.
  static void ShowPublicAccountInfo();

  // Shows UI for changing proxy settings.
  static void ShowProxySettings();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SystemTrayCommon);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_COMMON_H_
