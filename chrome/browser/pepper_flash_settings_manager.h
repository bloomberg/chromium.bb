// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
#pragma once

#include "base/basictypes.h"

class PluginPrefs;
class PrefService;

namespace webkit {
struct WebPluginInfo;
}

// PepperFlashSettingsManager communicates with a PPAPI broker process to
// read/write Pepper Flash settings.
class PepperFlashSettingsManager {
 public:
  // |plugin_info| will be updated if it is not NULL and the method returns
  // true.
  static bool IsPepperFlashInUse(PluginPrefs* plugin_prefs,
                                 webkit::WebPluginInfo* plugin_info);

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PepperFlashSettingsManager);
};

#endif  // CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
