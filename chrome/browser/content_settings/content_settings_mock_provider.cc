// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_mock_provider.h"

namespace content_settings {

MockProvider::MockProvider()
    : requesting_url_pattern_(ContentSettingsPattern()),
      embedding_url_pattern_(ContentSettingsPattern()),
      content_type_(CONTENT_SETTINGS_TYPE_COOKIES),
      resource_identifier_(""),
      setting_(CONTENT_SETTING_DEFAULT),
      read_only_(false) {}

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
      read_only_(read_only) {}

MockProvider::~MockProvider() {}

ContentSetting MockProvider::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  if (requesting_url_pattern_.Matches(primary_url) &&
      content_type_ == content_type &&
      resource_identifier_ == resource_identifier) {
    return setting_;
  }
  return CONTENT_SETTING_DEFAULT;
}

Value* MockProvider::GetContentSettingValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  ContentSetting setting = GetContentSetting(
      primary_url,
      secondary_url,
      content_type,
      resource_identifier);
  if (setting == CONTENT_SETTING_DEFAULT)
    return NULL;
  return Value::CreateIntegerValue(setting);
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

void MockProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
}

}  // namespace content_settings
