// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PRESENTATION_MODE_PREFS_H_
#define CHROME_BROWSER_UI_COCOA_PRESENTATION_MODE_PREFS_H_
#pragma once

#include "base/basictypes.h"

class PrefService;

class PresentationModePrefs {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PresentationModePrefs);
};

#endif  // CHROME_BROWSER_UI_COCOA_PRESENTATION_MODE_PREFS_H_
