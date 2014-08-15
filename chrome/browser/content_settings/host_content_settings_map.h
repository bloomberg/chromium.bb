// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/threading/platform_thread.h"
#include "base/tuple.h"
#include "chrome/browser/content_settings/content_settings_observer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

class ExtensionService;
class GURL;
class PrefService;

namespace base {
class Clock;
class Value;
}

namespace content_settings {
class ProviderInterface;
class PrefProvider;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class HostContentSettingsMap
    : public content_settings::Observer,
      public base::RefCountedThreadSafe<HostContentSettingsMap> {
 public:
  enum ProviderType {
    INTERNAL_EXTENSION_PROVIDER = 0,
    POLICY_PROVIDER,
    CUSTOM_EXTENSION_PROVIDER,
    PREF_PROVIDER,
    DEFAULT_PROVIDER,
    NUM_PROVIDER_TYPES,
  };

  HostContentSettingsMap(PrefService* prefs, bool incognito);

#if defined(ENABLE_EXTENSIONS)
  // In some cases, the ExtensionService is not available at the time the
  // HostContentSettingsMap is constructed. In these cases, we register the
  // service once it's available.
  void RegisterExtensionService(ExtensionService* extension_service);
#endif

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the default setting for a particular content type. If |provider_id|
  // is not NULL, the id of the provider which provided the default setting is
  // assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultContentSetting(ContentSettingsType content_type,
                                          std::string* provider_id) const;

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
  // If |info| is not NULL, then the |source| field of |info| is set to the
  // source of the returned |Value| (POLICY, EXTENSION, USER, ...) and the
  // |primary_pattern| and the |secondary_pattern| fields of |info| are set to
  // the patterns of the applying rule.  Note that certain internal schemes are
  // whitelisted. For whitelisted schemes the |source| field of |info| is set
  // the |SETTING_SOURCE_WHITELIST| and the |primary_pattern| and
  // |secondary_pattern| are set to a wildcard pattern.  If there is no content
  // setting, NULL is returned and the |source| field of |info| is set to
  // |SETTING_SOURCE_NONE|. The pattern fiels of |info| are set to empty
  // patterns.
  // The ownership of the resulting |Value| is transfered to the caller.
  // May be called on any thread.
  base::Value* GetWebsiteSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      content_settings::SettingInfo* info) const;

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

  // Sets the content |setting| for the given patterns, |content_type| and
  // |resource_identifier|. Setting the value to CONTENT_SETTING_DEFAULT causes
  // the default setting for that type to be used when loading pages matching
  // this pattern.
  // NOTICE: This is just a convenience method for content types that use
  // |CONTENT_SETTING| as their data type. For content types that use other
  // data types please use the method SetWebsiteSetting.
  //
  // This should only be called on the UI thread.
  void SetContentSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const std::string& resource_identifier,
                         ContentSetting setting);

  // Sets the |value| for the given patterns, |content_type| and
  // |resource_identifier|. Setting the value to NULL causes the default value
  // for that type to be used when loading pages matching this pattern.
  //
  // Takes ownership of the passed value.
  void SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const std::string& resource_identifier,
                         base::Value* value);

  // Convenience method to add a content setting for the given URLs, making sure
  // that there is no setting overriding it.
  //
  // This should only be called on the UI thread.
  void AddExceptionForURL(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType content_type,
                          ContentSetting setting);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  static bool IsValueAllowedForType(PrefService* prefs,
                                    const base::Value* value,
                                    ContentSettingsType content_type);
  static bool IsSettingAllowedForType(PrefService* prefs,
                                      ContentSetting setting,
                                      ContentSettingsType content_type);

  // Returns true if the values for content type are of type dictionary/map.
  static bool ContentTypeHasCompoundValue(ContentSettingsType type);

  // Detaches the HostContentSettingsMap from all Profile-related objects like
  // PrefService. This methods needs to be called before destroying the Profile.
  // Afterwards, none of the methods above that should only be called on the UI
  // thread should be called anymore.
  void ShutdownOnUIThread();

  // content_settings::Observer implementation.
  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier) OVERRIDE;

  // Returns true if we should allow all content types for this URL.  This is
  // true for various internal objects like chrome:// URLs, so UI and other
  // things users think of as "not webpages" don't break.
  static bool ShouldAllowAllContent(const GURL& primary_url,
                                    const GURL& secondary_url,
                                    ContentSettingsType content_type);

  // Returns the ProviderType associated with the given source string.
  // TODO(estade): I regret adding this. At the moment there are no legitimate
  // uses. We should stick to ProviderType rather than string so we don't have
  // to convert backwards.
  static ProviderType GetProviderTypeFromSource(const std::string& source);

  bool is_off_the_record() const {
    return is_off_the_record_;
  }

  // Returns a single |ContentSetting| which applies to the given URLs, just as
  // |GetContentSetting| does. If the setting is allowed, it also records the
  // last usage to preferences.
  //
  // This should only be called on the UI thread, unlike |GetContentSetting|.
  ContentSetting GetContentSettingAndMaybeUpdateLastUsage(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier);

  // Sets the last time that a given content type has been used for the pattern
  // which matches the URLs to the current time.
  void UpdateLastUsage(const GURL& primary_url,
                       const GURL& secondary_url,
                       ContentSettingsType content_type);

  // Sets the last time that a given content type has been used for a pattern
  // pair to the current time.
  void UpdateLastUsageByPattern(const ContentSettingsPattern& primary_pattern,
                                const ContentSettingsPattern& secondary_pattern,
                                ContentSettingsType content_type);

  // Returns the last time the pattern that matches the URL has requested
  // permission for the |content_type| setting.
  base::Time GetLastUsage(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType content_type);

  // Returns the last time the pattern has requested permission for the
  // |content_type| setting.
  base::Time GetLastUsageByPattern(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type);

  // Adds/removes an observer for content settings changes.
  void AddObserver(content_settings::Observer* observer);
  void RemoveObserver(content_settings::Observer* observer);

  // Passes ownership of |clock|.
  void SetPrefClockForTesting(scoped_ptr<base::Clock> clock);

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

  // Migrate the Clear on exit pref into equivalent content settings.
  void MigrateObsoleteClearOnExitPref();

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

  // Call UsedContentSettingsProviders() whenever you access
  // content_settings_providers_ (apart from initialization and
  // teardown), so that we can DCHECK in RegisterExtensionService that
  // it is not being called too late.
  void UsedContentSettingsProviders() const;

  content_settings::PrefProvider* GetPrefProvider();

#ifndef NDEBUG
  // This starts as the thread ID of the thread that constructs this
  // object, and remains until used by a different thread, at which
  // point it is set to base::kInvalidThreadId. This allows us to
  // DCHECK on unsafe usage of content_settings_providers_ (they
  // should be set up on a single thread, after which they are
  // immutable).
  mutable base::PlatformThreadId used_from_thread_id_;
#endif

  // Weak; owned by the Profile.
  PrefService* prefs_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Content setting providers. This is only modified at construction
  // time and by RegisterExtensionService, both of which should happen
  // before any other uses of it.
  ProviderMap content_settings_providers_;

  ObserverList<content_settings::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
