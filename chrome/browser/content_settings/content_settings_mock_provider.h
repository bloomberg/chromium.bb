// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/content_settings/content_settings_provider.h"

namespace content_settings {

// The class MockDefaultProvider is a mock for a default content settings
// provider.
class MockDefaultProvider : public DefaultProviderInterface {
 public:
  // Create a content settings provider that provides a given setting for a
  // given type.
  MockDefaultProvider(ContentSettingsType content_type,
                              ContentSetting setting,
                              bool is_managed,
                              bool can_override);
  virtual ~MockDefaultProvider();

  // DefaultProviderInterface implementation.
  virtual ContentSetting ProvideDefaultSetting(
      ContentSettingsType content_type) const;
  virtual void UpdateDefaultSetting(ContentSettingsType content_type,
                                    ContentSetting setting);
  virtual void ResetToDefaults();
  virtual bool DefaultSettingIsManaged(ContentSettingsType content_type) const;

 private:
  ContentSettingsType content_type_;
  ContentSetting setting_;
  bool is_managed_;
  bool can_override_;

  DISALLOW_COPY_AND_ASSIGN(MockDefaultProvider);
};

// The class MockProvider is a mock for a non default content settings provider.
class MockProvider : public ProviderInterface {
 public:
  MockProvider();
  MockProvider(ContentSettingsPattern requesting_url_pattern,
               ContentSettingsPattern embedding_url_pattern,
               ContentSettingsType content_type,
               ResourceIdentifier resource_identifier,
               ContentSetting setting,
               bool read_only);
  virtual ~MockProvider();

  // ProviderInterface implementation
  virtual ContentSetting GetContentSetting(
      const GURL& requesting_url,
      const GURL& embedding_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  // The MockProvider is only able to store one content setting. So every time
  // this method is called the previously set content settings is overwritten.
  virtual void SetContentSetting(
      const ContentSettingsPattern& requesting_url_pattern,
      const ContentSettingsPattern& embedding_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting);

  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const {}

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) {}

  virtual void ResetToDefaults() {}

  // Accessors
  void set_requesting_url_pattern(ContentSettingsPattern pattern) {
    requesting_url_pattern_ = pattern;
  }

  ContentSettingsPattern requesting_url_pattern() {
    return requesting_url_pattern_;
  }

  void set_embedding_url_pattern(ContentSettingsPattern pattern) {
    embedding_url_pattern_ = pattern;
  }

  ContentSettingsPattern embedding_url_pattern() {
    return embedding_url_pattern_;
  }

  void set_content_type(ContentSettingsType content_type) {
    content_type_ = content_type;
  }

  ContentSettingsType content_type() {
    return content_type_;
  }

  void set_resource_identifier(ResourceIdentifier resource_identifier) {
    resource_identifier_ = resource_identifier;
  }

  ResourceIdentifier resource_identifier() {
    return resource_identifier_;
  }

  void set_setting(ContentSetting setting) {
    setting_ = setting;
  }

  ContentSetting setting() {
    return setting_;
  }

  void set_read_only(bool read_only) {
    read_only_ = read_only;
  }

  bool read_only() {
    return read_only_;
  }

 private:
  ContentSettingsPattern requesting_url_pattern_;
  ContentSettingsPattern embedding_url_pattern_;
  ContentSettingsType content_type_;
  ResourceIdentifier resource_identifier_;
  ContentSetting setting_;
  bool read_only_;

  DISALLOW_COPY_AND_ASSIGN(MockProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_PROVIDER_H_
