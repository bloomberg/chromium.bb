// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mock_content_settings_provider.h"

MockContentSettingsProvider::MockContentSettingsProvider(
    ContentSettingsType content_type,
    ContentSetting setting,
    bool is_managed,
    bool can_override)
    : content_type_(content_type),
      setting_(setting),
      is_managed_(is_managed),
      can_override_(can_override) {
}

MockContentSettingsProvider::~MockContentSettingsProvider() {
}

bool MockContentSettingsProvider::CanProvideDefaultSetting(
    ContentSettingsType content_type) const {
  return content_type == content_type_;
}

ContentSetting MockContentSettingsProvider::ProvideDefaultSetting(
    ContentSettingsType content_type) const {
  return content_type == content_type_ ? setting_ : CONTENT_SETTING_DEFAULT;
}

void MockContentSettingsProvider::UpdateDefaultSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  if (can_override_ && content_type == content_type_)
    setting_ = setting;
}

bool MockContentSettingsProvider::DefaultSettingIsManaged(
    ContentSettingsType content_type) const {
  return content_type == content_type_ && is_managed_;
}

void MockContentSettingsProvider::ResetToDefaults() {
}
