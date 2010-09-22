// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class DictionaryValue;
class GURL;
class PrefService;
class Profile;

class HostContentSettingsMap
    : public NotificationObserver,
      public base::RefCountedThreadSafe<HostContentSettingsMap,
                                        ChromeThread::DeleteOnUIThread> {
 public:
  // A hostname pattern. See |IsValid| for a description of possible patterns.
  class Pattern {
   public:
    // Returns a pattern that matches the host of this URL and all subdomains.
    static Pattern FromURL(const GURL& url);

    // Returns a pattern that matches exactly this URL.
    static Pattern FromURLNoWildcard(const GURL& url);

    Pattern() {}

    explicit Pattern(const std::string& pattern) : pattern_(pattern) {}

    // True if this is a valid pattern. Valid patterns are
    //   - [*.]domain.tld (matches domain.tld and all sub-domains)
    //   - host (matches an exact hostname)
    //   - a.b.c.d (matches an exact IPv4 ip)
    //   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
    bool IsValid() const;

    // True if |url| matches this pattern.
    bool Matches(const GURL& url) const;

    // Returns a std::string representation of this pattern.
    const std::string& AsString() const { return pattern_; }

    bool operator==(const Pattern& other) const {
      return pattern_ == other.pattern_;
    }

   private:
    std::string pattern_;
  };

  // Details for the CONTENT_SETTINGS_CHANGED notification. This is sent when
  // content settings change for at least one host. If settings change for more
  // than one pattern in one user interaction, this will usually send a single
  // notification with update_all() returning true instead of one notification
  // for each pattern.
  class ContentSettingsDetails {
   public:
    // Update the setting that matches this pattern/content type/resource.
    ContentSettingsDetails(const Pattern& pattern,
                           ContentSettingsType type,
                           const std::string& resource_identifier)
        : pattern_(pattern),
          type_(type),
          resource_identifier_(resource_identifier) {}

    // The pattern whose settings have changed.
    const Pattern& pattern() const { return pattern_; }

    // True if all settings should be updated for the given type.
    bool update_all() const { return pattern_.AsString().empty(); }

    // The type of the pattern whose settings have changed.
    ContentSettingsType type() const { return type_; }

    // The resource identifier for the settings type that has changed.
    const std::string& resource_identifier() const {
      return resource_identifier_;
    }

    // True if all types should be updated. If update_all() is false, this will
    // be false as well (although the reverse does not hold true).
    bool update_all_types() const {
      return CONTENT_SETTINGS_TYPE_DEFAULT == type_;
    }

   private:
    Pattern pattern_;
    ContentSettingsType type_;
    std::string resource_identifier_;
  };


  typedef std::pair<Pattern, ContentSetting> PatternSettingPair;
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
  void SetContentSetting(const Pattern& pattern,
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

  struct ExtendedContentSettings {
    ContentSettings content_settings;
    ResourceContentSettings content_settings_for_resources;
  };

  typedef std::map<std::string, ExtendedContentSettings> HostContentSettings;

  // Sets the fields of |settings| based on the values in |dictionary|.
  void GetSettingsFromDictionary(const DictionaryValue* dictionary,
                                 ContentSettings* settings);

  // Populates |settings| based on the values in |dictionary|.
  void GetResourceSettingsFromDictionary(const DictionaryValue* dictionary,
                                         ResourceContentSettings* settings);

  // Forces the default settings to be explicitly set instead of themselves
  // being CONTENT_SETTING_DEFAULT.
  void ForceDefaultsToBeExplicit();

  // Returns true if |settings| consists entirely of CONTENT_SETTING_DEFAULT.
  bool AllDefault(const ExtendedContentSettings& settings) const;

  // Reads the default settings from the prefereces service. If |overwrite| is
  // true and the preference is missing, the local copy will be cleared as well.
  void ReadDefaultSettings(bool overwrite);

  // Reads the host exceptions from the prefereces service. If |overwrite| is
  // true and the preference is missing, the local copy will be cleared as well.
  void ReadExceptions(bool overwrite);

  // Informs observers that content settings have changed. Make sure that
  // |lock_| is not held when calling this, as listeners will usually call one
  // of the GetSettings functions in response, which would then lead to a
  // mutex deadlock.
  void NotifyObservers(const ContentSettingsDetails& details);

  void UnregisterObservers();

  // The profile we're associated with.
  Profile* profile_;

  NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Copies of the pref data, so that we can read it on the IO thread.
  ContentSettings default_content_settings_;
  HostContentSettings host_content_settings_;

  // Differences to the preference-stored host content settings for
  // off-the-record settings.
  HostContentSettings off_the_record_settings_;

  // Misc global settings.
  bool block_third_party_cookies_;
  bool block_nonsandboxed_plugins_;

  // Used around accesses to the settings objects to guarantee thread safety.
  mutable Lock lock_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

// Stream operator so HostContentSettingsMap::Pattern can be used in
// assertion statements.
inline std::ostream& operator<<(
    std::ostream& out, const HostContentSettingsMap::Pattern& pattern) {
  return out << pattern.AsString();
}

#endif  // CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
