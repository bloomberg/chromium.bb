// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for objects providing content setting rules.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
#pragma once

#define NO_RESOURCE_IDENTIFIER ""

#include <string>
#include <vector>

#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/common/content_settings.h"

class GURL;

namespace content_settings {

class DefaultProviderInterface {
 public:
  virtual ~DefaultProviderInterface() {}

  // Returns the default content setting this provider has for the given
  // |content_type|, or CONTENT_SETTING_DEFAULT if nothing be provided for this
  // type.
  virtual ContentSetting ProvideDefaultSetting(
      ContentSettingsType content_type) const = 0;

  // Notifies the provider that the host content settings map would like to
  // update the default setting for the given |content_type|. The provider may
  // ignore this.
  virtual void UpdateDefaultSetting(ContentSettingsType content_type,
                                    ContentSetting setting) = 0;

  // Resets the state of the provider to the default.
  virtual void ResetToDefaults() = 0;

  // True if the default setting for the |content_type| is policy managed, i.e.,
  // there shouldn't be any UI shown to modify this setting.
  virtual bool DefaultSettingIsManaged(
      ContentSettingsType content_type) const = 0;
};

class ProviderInterface {
 public:
  typedef std::string ResourceIdentifier;

  struct Rule {
    Rule() {}
    Rule(const ContentSettingsPattern& requesting_pattern,
         const ContentSettingsPattern& embedding_pattern,
         ContentSetting setting)
      : requesting_url_pattern(requesting_pattern),
        embedding_url_pattern(embedding_pattern),
        content_setting(setting) {}

    ContentSettingsPattern requesting_url_pattern;
    ContentSettingsPattern embedding_url_pattern;
    ContentSetting content_setting;
  };

  typedef std::vector<Rule> Rules;

  virtual ~ProviderInterface() {}

  // Returns true whether the content settings provider manages the
  // |content_type|.
  virtual bool ContentSettingsTypeIsManaged(
      ContentSettingsType content_type) = 0;

  // Returns a single ContentSetting which applies to a given |requesting_url|,
  // |embedding_url| pair or CONTENT_SETTING_DEFAULT, if no rule applies. For
  // ContentSettingsTypes that require a resource identifier to be specified,
  // the |resource_identifier| must be non-empty.
  //
  // This may be called on any thread.
  virtual ContentSetting GetContentSetting(
      const GURL& requesting_url,
      const GURL& embedding_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const = 0;

  // Sets the content setting for a particular |requesting_pattern|,
  // |embedding_pattern|, |content_type| tuple. For ContentSettingsTypes that
  // require a resource identifier to be specified, the |resource_identifier|
  // must be non-empty.
  //
  // This should only be called on the UI thread.
  virtual void SetContentSetting(
      const ContentSettingsPattern& requesting_url_pattern,
      const ContentSettingsPattern& embedding_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting) = 0;

  // For a given content type, returns all content setting rules with a
  // non-default setting, mapped to their actual settings.
  // |content_settings_rules| must be non-NULL. If this provider was created for
  // the incognito profile, it will only return those settings differing
  // from the corresponding regular provider. For ContentSettingsTypes that
  // require a resource identifier to be specified, the |resource_identifier|
  // must be non-empty.
  //
  // This may be called on any thread.
  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const = 0;

  // Resets all content settings for the given |content_type| to
  // CONTENT_SETTING_DEFAULT. For content types that require a resource
  // identifier all content settings for any resource identifieres of the given
  // |content_type| will be reset to CONTENT_SETTING_DEFAULT.
  //
  // This should only be called on the UI thread.
  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) = 0;

  // Resets all content settings to CONTENT_SETTINGS_DEFAULT.
  //
  // This should only be called on the UI thread.
  virtual void ResetToDefaults() = 0;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
