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
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

namespace content_settings {
class DefaultProviderInterface;
}  // namespace content_settings

class ContentSettingsDetails;
class DictionaryValue;
class GURL;
class PrefService;
class Profile;

class HostContentSettingsMap
    : public NotificationObserver,
      public base::RefCountedThreadSafe<HostContentSettingsMap,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  typedef std::pair<ContentSettingsPattern, ContentSetting> PatternSettingPair;
  typedef std::vector<PatternSettingPair> SettingsForOneType;

  explicit HostContentSettingsMap(Profile* profile);
  ~HostContentSettingsMap();

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the default setting for a particular content type.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultContentSetting(
      ContentSettingsType content_type) const;

  // Returns a single ContentSetting which applies to a given URL. Note that
  // certain internal schemes are whitelisted. For ContentSettingsTypes that
  // require an resource identifier to be specified, the |resource_identifier|
  // must be non-empty.
  //
  // This may be called on any thread.
  ContentSetting GetContentSetting(
      const GURL& url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const;

  // Returns a single ContentSetting which applies to a given URL or
  // CONTENT_SETTING_DEFAULT, if no exception applies. Note that certain
  // internal schemes are whitelisted. For ContentSettingsTypes that require an
  // resource identifier to be specified, the |resource_identifier| must be
  // non-empty.
  //
  // This may be called on any thread.
  ContentSetting GetNonDefaultContentSetting(
      const GURL& url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const;

  // Returns all ContentSettings which apply to a given URL. For content
  // setting types that require an additional resource identifier, the default
  // content setting is returned.
  //
  // This may be called on any thread.
  ContentSettings GetContentSettings(const GURL& url) const;

  // Returns all non-default ContentSettings which apply to a given URL. For
  // content setting types that require an additional resource identifier,
  // CONTENT_SETTING_DEFAULT is returned.
  //
  // This may be called on any thread.
  ContentSettings GetNonDefaultContentSettings(const GURL& url) const;

  // For a given content type, returns all patterns with a non-default setting,
  // mapped to their actual settings, in lexicographical order.  |settings|
  // must be a non-NULL outparam. If this map was created for the
  // off-the-record profile, it will only return those settings differing from
  // the main map. For ContentSettingsTypes that require an resource identifier
  // to be specified, the |resource_identifier| must be non-empty.
  //
  // This may be called on any thread.
  void GetSettingsForOneType(ContentSettingsType content_type,
                             const std::string& resource_identifier,
                             SettingsForOneType* settings) const;

  // Sets the default setting for a particular content type. This method must
  // not be invoked on an off-the-record map.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSettingsType content_type,
                                ContentSetting setting);

  // Sets the blocking setting for a particular pattern and content type.
  // Setting the value to CONTENT_SETTING_DEFAULT causes the default setting
  // for that type to be used when loading pages matching this pattern. For
  // ContentSettingsTypes that require an resource identifier to be specified,
  // the |resource_identifier| must be non-empty.
  //
  // This should only be called on the UI thread.
  void SetContentSetting(const ContentSettingsPattern& pattern,
                         ContentSettingsType content_type,
                         const std::string& resource_identifier,
                         ContentSetting setting);

  // Convenience method to add a content setting for a given URL, making sure
  // that there is no setting overriding it. For ContentSettingsTypes that
  // require an resource identifier to be specified, the |resource_identifier|
  // must be non-empty.
  //
  // This should only be called on the UI thread.
  void AddExceptionForURL(const GURL& url,
                          ContentSettingsType content_type,
                          const std::string& resource_identifier,
                          ContentSetting setting);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  // Whether the |content_type| requires an additional resource identifier for
  // accessing content settings.
  bool RequiresResourceIdentifier(ContentSettingsType content_type) const;

  // This setting trumps any host-specific settings.
  bool BlockThirdPartyCookies() const { return block_third_party_cookies_; }
  bool IsBlockThirdPartyCookiesManaged() const {
    return is_block_third_party_cookies_managed_;
  }

  // Sets whether we block all third-party cookies. This method must not be
  // invoked on an off-the-record map.
  //
  // This should only be called on the UI thread.
  void SetBlockThirdPartyCookies(bool block);

  bool GetBlockNonsandboxedPlugins() const {
    return block_nonsandboxed_plugins_;
  }

  void SetBlockNonsandboxedPlugins(bool block);

  // Resets all settings levels.
  //
  // This should only be called on the UI thread.
  void ResetToDefaults();

  // Returns true if the default setting for the |content_type| is managed.
  bool IsDefaultContentSettingManaged(ContentSettingsType content_type) const;

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<HostContentSettingsMap>;

  typedef std::pair<ContentSettingsType, std::string>
      ContentSettingsTypeResourceIdentifierPair;
  typedef std::map<ContentSettingsTypeResourceIdentifierPair, ContentSetting>
      ResourceContentSettings;

  struct ExtendedContentSettings;
  typedef std::map<std::string, ExtendedContentSettings> HostContentSettings;

  // Sets the fields of |settings| based on the values in |dictionary|.
  void GetSettingsFromDictionary(const DictionaryValue* dictionary,
                                 ContentSettings* settings);

  // Populates |settings| based on the values in |dictionary|.
  void GetResourceSettingsFromDictionary(const DictionaryValue* dictionary,
                                         ResourceContentSettings* settings);

  // Returns true if |settings| consists entirely of CONTENT_SETTING_DEFAULT.
  bool AllDefault(const ExtendedContentSettings& settings) const;

  // Reads the host exceptions from the prefereces service. If |overwrite| is
  // true and the preference is missing, the local copy will be cleared as well.
  void ReadExceptions(bool overwrite);

  // Informs observers that content settings have changed. Make sure that
  // |lock_| is not held when calling this, as listeners will usually call one
  // of the GetSettings functions in response, which would then lead to a
  // mutex deadlock.
  void NotifyObservers(const ContentSettingsDetails& details);

  void UnregisterObservers();

  // Various migration methods (old cookie, popup and per-host data gets
  // migrated to the new format).
  void MigrateObsoleteCookiePref(PrefService* prefs);
  void MigrateObsoletePopupsPref(PrefService* prefs);
  void MigrateObsoletePerhostPref(PrefService* prefs);

  // Converts all exceptions that have non-canonicalized pattern into
  // canonicalized pattern. If such pattern already exists, we just remove the
  // old exception.
  void CanonicalizeContentSettingsExceptions(DictionaryValue* settings);

  // The profile we're associated with.
  Profile* profile_;

  NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  // Content setting providers.
  std::vector<linked_ptr<content_settings::DefaultProviderInterface> >
      default_content_settings_providers_;

  // Used around accesses to the following objects to guarantee thread safety.
  mutable base::Lock lock_;

  // Copies of the pref data, so that we can read it on threads other than the
  // UI thread.
  HostContentSettings host_content_settings_;

  // Differences to the preference-stored host content settings for
  // off-the-record settings.
  HostContentSettings off_the_record_settings_;

  // Misc global settings.
  bool block_third_party_cookies_;
  bool is_block_third_party_cookies_managed_;
  bool block_nonsandboxed_plugins_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
