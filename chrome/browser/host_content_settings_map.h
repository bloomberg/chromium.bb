// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
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
    explicit ContentSettingsDetails(const Pattern& pattern)
        : pattern_(pattern), update_all_(false) {}

    explicit ContentSettingsDetails(bool update_all)
        : pattern_(), update_all_(update_all) {}

    // The pattern whose settings have changed.
    const Pattern& pattern() const { return pattern_; }

    // True if many settings changed at once.
    bool update_all() const { return update_all_; }

   private:
    Pattern pattern_;
    bool update_all_;
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
  // certain internal schemes are whitelisted.
  //
  // This may be called on any thread.
  ContentSetting GetContentSetting(const GURL& url,
                                   ContentSettingsType content_type) const;

  // Returns all ContentSettings which apply to a given URL.
  //
  // This may be called on any thread.
  ContentSettings GetContentSettings(const GURL& url) const;

  // For a given content type, returns all patterns with a non-default setting,
  // mapped to their actual settings, in lexicographical order.  |settings|
  // must be a non-NULL outparam. If this map was created for the
  // off-the-record profile, it will only return those settings differing from
  // the main map.
  //
  // This may be called on any thread.
  void GetSettingsForOneType(ContentSettingsType content_type,
                             SettingsForOneType* settings) const;

  // Sets the default setting for a particular content type. This method must
  // not be invoked on an off-the-record map.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSettingsType content_type,
                                ContentSetting setting);

  // Sets the blocking setting for a particular pattern and content type.
  // Setting the value to CONTENT_SETTING_DEFAULT causes the default setting for
  // that type to be used when loading pages matching this pattern.
  //
  // This should only be called on the UI thread.
  void SetContentSetting(const Pattern& pattern,
                         ContentSettingsType content_type,
                         ContentSetting setting);

  // Convenience method to add a content setting for a given URL, making sure
  // that there is no setting overriding it.
  // This should only be called on the UI thread.
  void AddExceptionForURL(const GURL& url,
                          ContentSettingsType content_type,
                          ContentSetting setting);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  // This setting trumps any host-specific settings.
  bool BlockThirdPartyCookies() const { return block_third_party_cookies_; }

  // Sets whether we block all third-party cookies. This method must not be
  // invoked on an off-the-record map.
  //
  // This should only be called on the UI thread.
  void SetBlockThirdPartyCookies(bool block);

  // Resets all settings levels.
  //
  // This should only be called on the UI thread.
  void ResetToDefaults();

  // Whether this settings map is associated with an OTR session.
  bool IsOffTheRecord();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<HostContentSettingsMap>;

  typedef std::map<std::string, ContentSettings> HostContentSettings;

  // The names of the ContentSettingsType values, for use with dictionary prefs.
  static const wchar_t* kTypeNames[CONTENT_SETTINGS_NUM_TYPES];

  // The default setting for each content type.
  static const ContentSetting kDefaultSettings[CONTENT_SETTINGS_NUM_TYPES];

  // Returns true if we should allow all content types for this URL.  This is
  // true for various internal objects like chrome:// URLs, so UI and other
  // things users think of as "not webpages" don't break.
  static bool ShouldAllowAllContent(const GURL& url);

  // Sets the fields of |settings| based on the values in |dictionary|.
  void GetSettingsFromDictionary(const DictionaryValue* dictionary,
                                 ContentSettings* settings);

  // Forces the default settings to be explicitly set instead of themselves
  // being CONTENT_SETTING_DEFAULT.
  void ForceDefaultsToBeExplicit();

  // Returns true if |settings| consists entirely of CONTENT_SETTING_DEFAULT.
  bool AllDefault(const ContentSettings& settings) const;

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

  // Copies of the pref data, so that we can read it on the IO thread.
  ContentSettings default_content_settings_;
  HostContentSettings host_content_settings_;

  // Differences to the preference-stored host content settings for
  // off-the-record settings.
  HostContentSettings off_the_record_settings_;

  // Misc global settings.
  bool block_third_party_cookies_;

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
