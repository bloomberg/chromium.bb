// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_H_
#define CHROME_COMMON_CONTENT_SETTINGS_H_
#pragma once

#include "chrome/common/content_settings_types.h"

// Different settings that can be assigned for a particular content type.  We
// give the user the ability to set these on a global and per-host basis.
enum ContentSetting {
  CONTENT_SETTING_DEFAULT = 0,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ASK,
  CONTENT_SETTING_SESSION_ONLY,
  CONTENT_SETTING_NUM_SETTINGS
};

// Range-checked conversion of an int to a ContentSetting, for use when reading
// prefs off disk.
ContentSetting IntToContentSetting(int content_setting);

// Aggregates the permissions for the different content types.
struct ContentSettings {
  ContentSettings() {
    for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      settings[i] = CONTENT_SETTING_DEFAULT;
  }

  explicit ContentSettings(ContentSetting default_setting) {
    for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      settings[i] = default_setting;
  }

  ContentSetting settings[CONTENT_SETTINGS_NUM_TYPES];
};

#endif  // CHROME_COMMON_CONTENT_SETTINGS_H_
