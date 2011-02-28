// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_mock_provider.h"

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

MockProvider::MockProvider()
    : requesting_url_pattern_(ContentSettingsPattern()),
      embedding_url_pattern_(ContentSettingsPattern()),
      content_type_(CONTENT_SETTINGS_TYPE_COOKIES),
      resource_identifier_(""),
      setting_(CONTENT_SETTING_DEFAULT),
      read_only_(false),
      is_managed_(false) {}

MockProvider::MockProvider(ContentSettingsPattern requesting_url_pattern,
                           ContentSettingsPattern embedding_url_pattern,
                           ContentSettingsType content_type,
                           ResourceIdentifier resource_identifier,
                           ContentSetting setting,
                           bool read_only,
                           bool is_managed)
    : requesting_url_pattern_(requesting_url_pattern),
      embedding_url_pattern_(embedding_url_pattern),
      content_type_(content_type),
      resource_identifier_(resource_identifier),
      setting_(setting),
      read_only_(read_only),
      is_managed_(is_managed) {}

MockProvider::~MockProvider() {}

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

bool MockProvider::ContentSettingsTypeIsManaged(ContentSettingsType type) {
  if (type == content_type_) {
    return is_managed_;
  }
  return false;
}

}  // namespace content_settings
