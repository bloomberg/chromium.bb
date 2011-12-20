// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings.h"

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS)) ?
      CONTENT_SETTING_DEFAULT : static_cast<ContentSetting>(content_setting);
}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting,
    const std::string& source,
    bool incognito)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      setting(setting),
      source(source),
      incognito(incognito) {}

ContentSettingPatternSource::ContentSettingPatternSource()
    : setting(CONTENT_SETTING_DEFAULT), incognito(false) {
}

RendererContentSettingRules::RendererContentSettingRules() {}

RendererContentSettingRules::~RendererContentSettingRules() {}
