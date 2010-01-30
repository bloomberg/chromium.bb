// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_H_
#define CHROME_COMMON_CONTENT_SETTINGS_H_

#include "chrome/common/content_settings_types.h"

// Different settings that can be assigned for a particular content type.  We
// give the user the ability to set these on a global and per-host basis.
enum ContentSetting {
  CONTENT_SETTING_FIRST_SETTING = 0,
  CONTENT_SETTING_DEFAULT = CONTENT_SETTING_FIRST_SETTING,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ASK,
  CONTENT_SETTING_NUM_SETTINGS
};

// Aggregates the permissions for the different content types.
struct ContentSettings {
  ContentSettings() {
    for (int i = CONTENT_SETTINGS_FIRST_TYPE; i < CONTENT_SETTINGS_NUM_TYPES;
         ++i)
      settings[i] = CONTENT_SETTING_DEFAULT;
  }

  ContentSetting settings[CONTENT_SETTINGS_NUM_TYPES];
};

#endif  // CHROME_COMMON_CONTENT_SETTINGS_H_
