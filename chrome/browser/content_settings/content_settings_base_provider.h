// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_BASE_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_BASE_PROVIDER_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/content_settings_provider.h"

namespace content_settings {

typedef std::pair<ContentSettingsType, std::string>
    ContentSettingsTypeResourceIdentifierPair;
typedef std::map<ContentSettingsTypeResourceIdentifierPair, ContentSetting>
    ResourceContentSettings;

struct ExtendedContentSettings {
  ContentSettings content_settings;
  ResourceContentSettings content_settings_for_resources;
};

// Map for ContentSettings.
typedef std::map<std::string, ExtendedContentSettings> HostContentSettings;

// BaseProvider is the abstract base class for all content-settings-provider
// classes.
class BaseProvider : public ProviderInterface {
 public:
  // Maps CONTENT_SETTING_ASK for the CONTENT_SETTINGS_TYPE_PLUGINS to
  // CONTENT_SETTING_BLOCK if click-to-play is not enabled.
  static ContentSetting ClickToPlayFixup(ContentSettingsType content_type,
                                         ContentSetting setting);

  explicit BaseProvider(bool is_otr)
      : is_off_the_record_(is_otr) {
  }
  virtual ~BaseProvider() {}

  // Initializes the Provider.
  virtual void Init() = 0;

  // ProviderInterface Implementation
  virtual bool ContentSettingsTypeIsManaged(
      ContentSettingsType content_type) = 0;

  virtual ContentSetting GetContentSetting(
      const GURL& requesting_url,
      const GURL& embedding_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  virtual void SetContentSetting(
      const ContentSettingsPattern& requesting_pattern,
      const ContentSettingsPattern& embedding_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting) = 0;

  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) = 0;

  virtual void ResetToDefaults() = 0;

 protected:
  // Returns true if the |content_type| requires a resource identifier.
  bool RequiresResourceIdentifier(
      ContentSettingsType content_type) const;

  // Returns true if the passed |settings| object contains only
  // CONTENT_SETTING_DEFAULT values.
  bool AllDefault(const ExtendedContentSettings& settings) const;

  // TODO(markusheintz): LEGACY method. Will be removed in a future re-factoring
  // step.
  ContentSettings GetNonDefaultContentSettings(const GURL& url) const;

  // Accessors
  HostContentSettings* host_content_settings() {
    return &host_content_settings_;
  }

  HostContentSettings* off_the_record_settings() {
    return &off_the_record_settings_;
  }

  base::Lock& lock() const {
    return lock_;
  }

  bool is_off_the_record() const {
    return is_off_the_record_;
  }

 private:
  // Copies of the pref data, so that we can read it on threads other than the
  // UI thread.
  HostContentSettings host_content_settings_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Differences to the preference-stored host content settings for
  // off-the-record settings.
  HostContentSettings off_the_record_settings_;

  // Used around accesses to the content_settings_ object to guarantee
  // thread safety.
  mutable base::Lock lock_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_BASE_PROVIDER_H_
