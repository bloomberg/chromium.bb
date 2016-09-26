// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_COMMON_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_COMMON_H_

#include "base/macros.h"

// System tray code shared between classic ash SystemTrayDelegateChromeos and
// mustash SystemTrayClient.
class SystemTrayCommon {
 public:
  // Shows the settings related to date, timezone etc.
  static void ShowDateSettings();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SystemTrayCommon);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_COMMON_H_
