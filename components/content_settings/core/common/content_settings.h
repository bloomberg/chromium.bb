// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_

#include <string>
#include <vector>

#include "components/content_settings/core/common/content_settings_pattern.h"

// Different settings that can be assigned for a particular content type.  We
// give the user the ability to set these on a global and per-origin basis.
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

struct RendererContentSettingRules {
  RendererContentSettingRules();
  ~RendererContentSettingRules();
  ContentSettingsForOneType image_rules;
  ContentSettingsForOneType script_rules;
};

namespace content_settings {

// Enum containing the various source for content settings. Settings can be
// set by policy, extension or the user. Certain (internal) schemes are
// whilelisted. For whilelisted schemes the source is
// |SETTING_SOURCE_WHITELIST|.
enum SettingSource {
  SETTING_SOURCE_NONE,
  SETTING_SOURCE_POLICY,
  SETTING_SOURCE_EXTENSION,
  SETTING_SOURCE_USER,
  SETTING_SOURCE_WHITELIST,
};

// |SettingInfo| provides meta data for content setting values. |source|
// contains the source of a value. |primary_pattern| and |secondary_pattern|
// contains the patterns of the appling rule.
struct SettingInfo {
  SettingSource source;
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_
