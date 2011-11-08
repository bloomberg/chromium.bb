// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_H_
#define CHROME_COMMON_CONTENT_SETTINGS_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/common/content_settings_pattern.h"
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
  ContentSettings();
  explicit ContentSettings(ContentSetting default_setting);

  ContentSetting settings[CONTENT_SETTINGS_NUM_TYPES];
};

struct ContentSettingPatternSource {
  ContentSettingPatternSource(const ContentSettingsPattern& primary_pattern,
                              const ContentSettingsPattern& secondary_patttern,
                              ContentSetting setting,
                              const std::string& source,
                              bool incognito);
  ContentSettingPatternSource();
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  ContentSetting setting;
  std::string source;
  bool incognito;
};

typedef std::vector<ContentSettingPatternSource> ContentSettingsForOneType;

#endif  // CHROME_COMMON_CONTENT_SETTINGS_H_
