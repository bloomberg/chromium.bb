// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/tuple.h"
#include "chrome/browser/content_settings/content_settings_observer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"

namespace base {
class Value;
}  // namespace base

namespace content_settings {
class ProviderInterface;
}  // namespace content_settings

class ExtensionService;
class GURL;
class PrefService;

class HostContentSettingsMap
    : public content_settings::Observer,
      public base::RefCountedThreadSafe<HostContentSettingsMap> {
 public:
  enum ProviderType {
    POLICY_PROVIDER = 0,
    EXTENSION_PROVIDER = 1,
    PREF_PROVIDER,
    DEFAULT_PROVIDER,
    NUM_PROVIDER_TYPES,
  };

  HostContentSettingsMap(PrefService* prefs,
                         ExtensionService* extension_service,
                         bool incognito);

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the default setting for a particular content type. If |provider_id|
  // is not NULL, the id of the provider which provided the default setting is
  // assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultContentSetting(ContentSettingsType content_type,
                                          std::string* provider_id) const;

  // Returns the default settings for all content types.
  //
  // This may be called on any thread.
  ContentSettings GetDefaultContentSettings() const;

  // Returns a single |ContentSetting| which applies to the given URLs.  Note
  // that certain internal schemes are whitelisted. For |CONTENT_TYPE_COOKIES|,
  // |CookieSettings| should be used instead. For content types that can't be
  // converted to a |ContentSetting|, |GetContentSettingValue| should be called.
  // If there is no content setting, returns CONTENT_SETTING_DEFAULT.
  //
  // May be called on any thread.
  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const;

  // Returns a single content setting |Value| which applies to the given URLs.
  // If |primary_pattern| and |secondary_pattern| are not NULL, they are set to
  // the patterns of the applying rule.
  // Note that certain internal schemes are whitelisted.
  // If there is no content setting, returns NULL and leaves |primary_pattern|
  // and |secondary_pattern| unchanged.
  // Otherwise transfers ownership of the resulting |Value| to the caller.
  // May be called on any thread.
  base::Value* GetContentSettingValue(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern) const;

  // Returns all ContentSettings which apply to the given |primary_url|. For
  // content setting types that require an additional resource identifier, the
  // default content setting is returned.
  //
  // This may be called on any thread.
  ContentSettings GetContentSettings(
      const GURL& primary_url) const;

  // For a given content type, returns all patterns with a non-default setting,
  // mapped to their actual settings, in the precedence order of the rules.
  // |settings| must be a non-NULL outparam.
  //
  // This may be called on any thread.
  void GetSettingsForOneType(ContentSettingsType content_type,
                             const std::string& resource_identifier,
                             ContentSettingsForOneType* settings) const;

  // Sets the default setting for a particular content type. This method must
  // not be invoked on an incognito map.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSettingsType content_type,
                                ContentSetting setting);

  // Sets the content setting for the given patterns and content type.
  // Setting the value to CONTENT_SETTING_DEFAULT causes the default setting
  // for that type to be used when loading pages matching this pattern. For
  // ContentSettingsTypes that require an resource identifier to be specified,
  // the |resource_identifier| must be non-empty.
  //
  // This should only be called on the UI thread.
  void SetContentSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const std::string& resource_identifier,
                         ContentSetting setting);

  // Convenience method to add a content setting for the given URLs, making sure
  // that there is no setting overriding it. For ContentSettingsTypes that
  // require an resource identifier to be specified, the |resource_identifier|
  // must be non-empty.
  //
  // This should only be called on the UI thread.
  void AddExceptionForURL(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType content_type,
                          const std::string& resource_identifier,
                          ContentSetting setting);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  static bool IsSettingAllowedForType(ContentSetting setting,
                                      ContentSettingsType content_type);

  // Detaches the HostContentSettingsMap from all Profile-related objects like
  // PrefService. This methods needs to be called before destroying the Profile.
  // Afterwards, none of the methods above that should only be called on the UI
  // thread should be called anymore.
  void ShutdownOnUIThread();

  // content_settings::Observer implementation.
  virtual void OnContentSettingChanged(
      ContentSettingsPattern primary_pattern,
      ContentSettingsPattern secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier);

  // Returns true if we should allow all content types for this URL.  This is
  // true for various internal objects like chrome:// URLs, so UI and other
  // things users think of as "not webpages" don't break.
  static bool ShouldAllowAllContent(const GURL& primary_url,
                                    const GURL& secondary_url,
                                    ContentSettingsType content_type);

 private:
  friend class base::RefCountedThreadSafe<HostContentSettingsMap>;
  friend class HostContentSettingsMapTest_NonDefaultSettings_Test;

  typedef std::map<ProviderType, content_settings::ProviderInterface*>
      ProviderMap;
  typedef ProviderMap::iterator ProviderIterator;
  typedef ProviderMap::const_iterator ConstProviderIterator;

  virtual ~HostContentSettingsMap();

  ContentSetting GetDefaultContentSettingFromProvider(
      ContentSettingsType content_type,
      content_settings::ProviderInterface* provider) const;

  // Various migration methods (old cookie, popup and per-host data gets
  // migrated to the new format).
  void MigrateObsoleteCookiePref();

  // Adds content settings for |content_type| and |resource_identifier|,
  // provided by |provider|, into |settings|. If |incognito| is true, adds only
  // the content settings which are applicable to the incognito mode and differ
  // from the normal mode. Otherwise, adds the content settings for the normal
  // mode.
  void AddSettingsForOneType(
      const content_settings::ProviderInterface* provider,
      ProviderType provider_type,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      ContentSettingsForOneType* settings,
      bool incognito) const;

  // Weak; owned by the Profile.
  PrefService* prefs_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  // Content setting providers.
  ProviderMap content_settings_providers_;

  // Used around accesses to the following objects to guarantee thread safety.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
