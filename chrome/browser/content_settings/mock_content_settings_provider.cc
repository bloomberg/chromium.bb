// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mock_content_settings_provider.h"

namespace content_settings {

MockDefaultProvider::MockDefaultProvider(
    ContentSettingsType content_type,
    ContentSetting setting,
    bool is_managed,
    bool can_override)
    : content_type_(content_type),
      setting_(setting),
      is_managed_(is_managed),
      can_override_(can_override) {
}

MockDefaultProvider::~MockDefaultProvider() {
}

ContentSetting MockDefaultProvider::ProvideDefaultSetting(
    ContentSettingsType content_type) const {
  return content_type == content_type_ ? setting_ : CONTENT_SETTING_DEFAULT;
}

void MockDefaultProvider::UpdateDefaultSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  if (can_override_ && content_type == content_type_)
    setting_ = setting;
}

bool MockDefaultProvider::DefaultSettingIsManaged(
    ContentSettingsType content_type) const {
  return content_type == content_type_ && is_managed_;
}

void MockDefaultProvider::ResetToDefaults() {
}

ContentSetting MockProvider::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  if (requesting_url_pattern_.Matches(requesting_url) &&
      content_type_ == content_type &&
      resource_identifier_ == resource_identifier) {
    return setting_;
  }
  return CONTENT_SETTING_DEFAULT;
}

void MockProvider::SetContentSetting(
    const ContentSettingsPattern& requesting_url_pattern,
    const ContentSettingsPattern& embedding_url_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting content_setting) {
  if (read_only_)
    return;
  requesting_url_pattern_ = ContentSettingsPattern(requesting_url_pattern);
  embedding_url_pattern_ = ContentSettingsPattern(embedding_url_pattern);
  content_type_ = content_type;
  resource_identifier_ = resource_identifier;
  setting_ = content_setting;
}

}  // namespace content_settings
