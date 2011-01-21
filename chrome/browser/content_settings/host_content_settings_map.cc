// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/policy_content_settings_provider.h"
#include "chrome/browser/content_settings/pref_content_settings_provider.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/base/static_cookie_policy.h"

namespace {

// The preference keys where resource identifiers are stored for
// ContentSettingsType values that support resource identifiers.
const char* kResourceTypeNames[CONTENT_SETTINGS_NUM_TYPES] = {
  NULL,
  NULL,
  NULL,
  "per_plugin",
  NULL,
  NULL,  // Not used for Geolocation
  NULL,  // Not used for Notifications
};

// The names of the ContentSettingsType values, for use with dictionary prefs.
const char* kTypeNames[CONTENT_SETTINGS_NUM_TYPES] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  NULL,  // Not used for Geolocation
  NULL,  // Not used for Notifications
};

// True if a given content settings type requires additional resource
// identifiers.
const bool kRequiresResourceIdentifier[CONTENT_SETTINGS_NUM_TYPES] = {
  false,  // CONTENT_SETTINGS_TYPE_COOKIES
  false,  // CONTENT_SETTINGS_TYPE_IMAGES
  false,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  true,   // CONTENT_SETTINGS_TYPE_PLUGINS
  false,  // CONTENT_SETTINGS_TYPE_POPUPS
  false,  // Not used for Geolocation
  false,  // Not used for Notifications
};

// Returns true if we should allow all content types for this URL.  This is
// true for various internal objects like chrome:// URLs, so UI and other
// things users think of as "not webpages" don't break.
static bool ShouldAllowAllContent(const GURL& url) {
  return url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme) ||
         url.SchemeIs(chrome::kGearsScheme) ||
         url.SchemeIs(chrome::kUserScriptScheme);
}

// Map ASK for the plugins content type to BLOCK if click-to-play is
// not enabled.
ContentSetting ClickToPlayFixup(ContentSettingsType content_type,
                                ContentSetting setting) {
  if (setting == CONTENT_SETTING_ASK &&
      content_type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay)) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

typedef std::vector<linked_ptr<ContentSettingsProviderInterface> >::iterator
    provider_iterator;
typedef
    std::vector<linked_ptr<ContentSettingsProviderInterface> >::const_iterator
        const_provider_iterator;

}  // namespace


struct HostContentSettingsMap::ExtendedContentSettings {
  ContentSettings content_settings;
  ResourceContentSettings content_settings_for_resources;
};

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile),
      is_off_the_record_(profile_->IsOffTheRecord()),
      updating_preferences_(false),
      block_third_party_cookies_(false),
      is_block_third_party_cookies_managed_(false) {
  // The order in which the content settings providers are created is critical,
  // as providers that are further down in the list (i.e. added later) override
  // providers further up.
  content_settings_providers_.push_back(
      linked_ptr<ContentSettingsProviderInterface>(
          new PrefContentSettingsProvider(profile)));
  content_settings_providers_.push_back(
      linked_ptr<ContentSettingsProviderInterface>(
          new PolicyContentSettingsProvider(profile)));

  PrefService* prefs = profile_->GetPrefs();

  MigrateObsoleteCookiePref(prefs);

  MigrateObsoletePopupsPref(prefs);

  MigrateObsoletePerhostPref(prefs);

  // Read misc. global settings.
  block_third_party_cookies_ =
      prefs->GetBoolean(prefs::kBlockThirdPartyCookies);
  is_block_third_party_cookies_managed_ =
      prefs->IsManagedPreference(prefs::kBlockThirdPartyCookies);
  block_nonsandboxed_plugins_ =
      prefs->GetBoolean(prefs::kBlockNonsandboxedPlugins);

  // Verify preferences version.
  if (!prefs->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs->SetInteger(prefs::kContentSettingsVersion,
                      ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    LOG(ERROR) << "Unknown content settings version in preferences.";
    return;
  }

  // Read exceptions.
  ReadExceptions(false);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kContentSettingsPatterns, this);
  pref_change_registrar_.Add(prefs::kBlockThirdPartyCookies, this);
  pref_change_registrar_.Add(prefs::kBlockNonsandboxedPlugins, this);
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion);
  prefs->RegisterDictionaryPref(prefs::kContentSettingsPatterns);
  prefs->RegisterBooleanPref(prefs::kBlockThirdPartyCookies, false);
  prefs->RegisterBooleanPref(prefs::kBlockNonsandboxedPlugins, false);
  prefs->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex, 0);

  // Obsolete prefs, for migration:
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::StaticCookiePolicy::ALLOW_ALL_COOKIES);
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts);
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type) const {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  for (const_provider_iterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end(); ++provider) {
    if (!(*provider)->CanProvideDefaultSetting(content_type))
      continue;
    ContentSetting provided_setting =
        (*provider)->ProvideDefaultSetting(content_type);
    if (provided_setting != CONTENT_SETTING_DEFAULT)
      setting = provided_setting;
  }
  // The method GetDefaultContentSetting always has to return an explicit
  // value that is to be used as default. We here rely on the
  // PrefContentSettingProvider to always provide a value.
  CHECK_NE(CONTENT_SETTING_DEFAULT, setting);
  return setting;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  ContentSetting setting = GetNonDefaultContentSetting(url,
                                                       content_type,
                                                       resource_identifier);
  if (setting == CONTENT_SETTING_DEFAULT ||
      IsDefaultContentSettingManaged(content_type)) {
    return GetDefaultContentSetting(content_type);
  }
  return setting;
}

ContentSetting HostContentSettingsMap::GetNonDefaultContentSetting(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  if (ShouldAllowAllContent(url))
    return CONTENT_SETTING_ALLOW;

  if (!RequiresResourceIdentifier(content_type))
    return GetNonDefaultContentSettings(url).settings[content_type];

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableResourceContentSettings)) {
    DCHECK(!resource_identifier.empty());
  }

  // Host content settings are ignored if the default_content_setting is
  // managed.
  if (IsDefaultContentSettingManaged(content_type)) {
    return GetDefaultContentSetting(content_type);
  }

  base::AutoLock auto_lock(lock_);

  const std::string host(net::GetHostOrSpecFromURL(url));
  ContentSettingsTypeResourceIdentifierPair
      requested_setting(content_type, resource_identifier);

  // Check for exact matches first.
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  if (i != host_content_settings_.end() &&
      i->second.content_settings_for_resources.find(requested_setting) !=
      i->second.content_settings_for_resources.end()) {
    return i->second.content_settings_for_resources.find(
        requested_setting)->second;
  }

  // If this map is not for an off-the-record profile, these searches will never
  // match. The additional off-the-record exceptions always overwrite the
  // regular ones.
  i = off_the_record_settings_.find(host);
  if (i != off_the_record_settings_.end() &&
      i->second.content_settings_for_resources.find(requested_setting) !=
      i->second.content_settings_for_resources.end()) {
    return i->second.content_settings_for_resources.find(
        requested_setting)->second;
  }

  // Match patterns starting with the most concrete pattern match.
  for (std::string key =
       std::string(ContentSettingsPattern::kDomainWildcard) + host; ; ) {
    HostContentSettings::const_iterator i(off_the_record_settings_.find(key));
    if (i != off_the_record_settings_.end() &&
        i->second.content_settings_for_resources.find(requested_setting) !=
        i->second.content_settings_for_resources.end()) {
      return i->second.content_settings_for_resources.find(
          requested_setting)->second;
    }

    i = host_content_settings_.find(key);
    if (i != host_content_settings_.end() &&
        i->second.content_settings_for_resources.find(requested_setting) !=
        i->second.content_settings_for_resources.end()) {
      return i->second.content_settings_for_resources.find(
          requested_setting)->second;
    }

    const size_t next_dot =
        key.find('.', ContentSettingsPattern::kDomainWildcardLength);
    if (next_dot == std::string::npos)
      break;
    key.erase(ContentSettingsPattern::kDomainWildcardLength,
              next_dot - ContentSettingsPattern::kDomainWildcardLength + 1);
  }

  return CONTENT_SETTING_DEFAULT;
}

ContentSettings HostContentSettingsMap::GetContentSettings(
    const GURL& url) const {
  ContentSettings output = GetNonDefaultContentSettings(url);

  // If we require a resource identifier, set the content settings to default,
  // otherwise make the defaults explicit.
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    if (RequiresResourceIdentifier(ContentSettingsType(j))) {
      output.settings[j] = CONTENT_SETTING_DEFAULT;
    } else {
      // A managed default content setting has the highest priority and hence
      // will overwrite any previously set value.
      if ((output.settings[j] == CONTENT_SETTING_DEFAULT) ||
          IsDefaultContentSettingManaged(ContentSettingsType(j))) {
        output.settings[j] = GetDefaultContentSetting(ContentSettingsType(j));
      }
    }
  }
  return output;
}

ContentSettings HostContentSettingsMap::GetNonDefaultContentSettings(
    const GURL& url) const {
  if (ShouldAllowAllContent(url))
      return ContentSettings(CONTENT_SETTING_ALLOW);

  base::AutoLock auto_lock(lock_);

  const std::string host(net::GetHostOrSpecFromURL(url));
  ContentSettings output;
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
    output.settings[j] = CONTENT_SETTING_DEFAULT;

  // Check for exact matches first.
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  if (i != host_content_settings_.end())
    output = i->second.content_settings;

  // If this map is not for an off-the-record profile, these searches will never
  // match. The additional off-the-record exceptions always overwrite the
  // regular ones.
  i = off_the_record_settings_.find(host);
  if (i != off_the_record_settings_.end()) {
    for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
      if (i->second.content_settings.settings[j] != CONTENT_SETTING_DEFAULT)
        output.settings[j] = i->second.content_settings.settings[j];
  }

  // Match patterns starting with the most concrete pattern match.
  for (std::string key =
       std::string(ContentSettingsPattern::kDomainWildcard) + host; ; ) {
    HostContentSettings::const_iterator i(off_the_record_settings_.find(key));
    if (i != off_the_record_settings_.end()) {
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
        if (output.settings[j] == CONTENT_SETTING_DEFAULT)
          output.settings[j] = i->second.content_settings.settings[j];
      }
    }
    i = host_content_settings_.find(key);
    if (i != host_content_settings_.end()) {
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
        if (output.settings[j] == CONTENT_SETTING_DEFAULT)
          output.settings[j] = i->second.content_settings.settings[j];
      }
    }
    const size_t next_dot =
        key.find('.', ContentSettingsPattern::kDomainWildcardLength);
    if (next_dot == std::string::npos)
      break;
    key.erase(ContentSettingsPattern::kDomainWildcardLength,
              next_dot - ContentSettingsPattern::kDomainWildcardLength + 1);
  }

  return output;
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    SettingsForOneType* settings) const {
  DCHECK(RequiresResourceIdentifier(content_type) !=
         resource_identifier.empty());
  DCHECK(settings);
  settings->clear();

  const HostContentSettings* map_to_return =
      is_off_the_record_ ? &off_the_record_settings_ : &host_content_settings_;
  ContentSettingsTypeResourceIdentifierPair
      requested_setting(content_type, resource_identifier);

  base::AutoLock auto_lock(lock_);
  for (HostContentSettings::const_iterator i(map_to_return->begin());
       i != map_to_return->end(); ++i) {
    ContentSetting setting;
    if (RequiresResourceIdentifier(content_type)) {
      if (i->second.content_settings_for_resources.find(requested_setting) !=
          i->second.content_settings_for_resources.end())
        setting = i->second.content_settings_for_resources.find(
            requested_setting)->second;
      else
        setting = CONTENT_SETTING_DEFAULT;
    } else {
     setting = i->second.content_settings.settings[content_type];
    }
    if (setting != CONTENT_SETTING_DEFAULT) {
      // Use of push_back() relies on the map iterator traversing in order of
      // ascending keys.
      settings->push_back(
          std::make_pair(ContentSettingsPattern(i->first), setting));
    }
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_PLUGINS ||
         setting != CONTENT_SETTING_ASK ||
         CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableClickToPlay));

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  for (provider_iterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end(); ++provider) {
    (*provider)->UpdateDefaultSetting(content_type, setting);
  }
}

void HostContentSettingsMap::SetContentSetting(
    const ContentSettingsPattern& original_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(RequiresResourceIdentifier(content_type),
            resource_identifier.empty());
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_PLUGINS ||
         setting != CONTENT_SETTING_ASK ||
         CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableClickToPlay));

  const ContentSettingsPattern pattern(original_pattern.CanonicalizePattern());

  bool early_exit = false;
  std::string pattern_str(pattern.AsString());
  PrefService* prefs = NULL;
  DictionaryValue* all_settings_dictionary = NULL;
  HostContentSettings* map_to_modify = &off_the_record_settings_;
  if (!is_off_the_record_) {
    prefs = profile_->GetPrefs();
    all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);
    map_to_modify = &host_content_settings_;
  }

  {
    base::AutoLock auto_lock(lock_);
    if (!map_to_modify->count(pattern_str))
      (*map_to_modify)[pattern_str].content_settings = ContentSettings();
    HostContentSettings::iterator
        i(map_to_modify->find(pattern_str));
    ContentSettings& settings = i->second.content_settings;
    if (RequiresResourceIdentifier(content_type)) {
      settings.settings[content_type] = CONTENT_SETTING_DEFAULT;
      if (setting != CONTENT_SETTING_DEFAULT) {
        i->second.content_settings_for_resources[
            ContentSettingsTypeResourceIdentifierPair(content_type,
            resource_identifier)] = setting;
      } else {
        i->second.content_settings_for_resources.erase(
            ContentSettingsTypeResourceIdentifierPair(content_type,
            resource_identifier));
      }
    } else {
      settings.settings[content_type] = setting;
    }
    if (AllDefault(i->second)) {
      map_to_modify->erase(i);
      if (all_settings_dictionary)
        all_settings_dictionary->RemoveWithoutPathExpansion(pattern_str, NULL);

      // We can't just return because |NotifyObservers()| needs to be called,
      // without |lock_| being held.
      early_exit = true;
    }
  }

  if (!early_exit && all_settings_dictionary) {
    DictionaryValue* host_settings_dictionary = NULL;
    bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
        pattern_str, &host_settings_dictionary);
    if (!found) {
      host_settings_dictionary = new DictionaryValue;
      all_settings_dictionary->SetWithoutPathExpansion(
          pattern_str, host_settings_dictionary);
      DCHECK_NE(setting, CONTENT_SETTING_DEFAULT);
    }
    if (RequiresResourceIdentifier(content_type)) {
      std::string dictionary_path(kResourceTypeNames[content_type]);
      DictionaryValue* resource_dictionary = NULL;
      found = host_settings_dictionary->GetDictionary(
          dictionary_path, &resource_dictionary);
      if (!found) {
        resource_dictionary = new DictionaryValue;
        host_settings_dictionary->Set(dictionary_path, resource_dictionary);
      }
      if (setting == CONTENT_SETTING_DEFAULT) {
        resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                        NULL);
      } else {
        resource_dictionary->SetWithoutPathExpansion(
            resource_identifier, Value::CreateIntegerValue(setting));
      }
    } else {
      std::string dictionary_path(kTypeNames[content_type]);
      if (setting == CONTENT_SETTING_DEFAULT) {
        host_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                             NULL);
      } else {
        host_settings_dictionary->SetWithoutPathExpansion(
            dictionary_path, Value::CreateIntegerValue(setting));
      }
    }
  }

  updating_preferences_ = true;
  if (!is_off_the_record_)
    ScopedPrefUpdate update(prefs, prefs::kContentSettingsPatterns);
  updating_preferences_ = false;

  NotifyObservers(ContentSettingsDetails(pattern, content_type, ""));
}

void HostContentSettingsMap::AddExceptionForURL(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  // Make sure there is no entry that would override the pattern we are about
  // to insert for exactly this URL.
  SetContentSetting(ContentSettingsPattern::FromURLNoWildcard(url),
                    content_type,
                    resource_identifier,
                    CONTENT_SETTING_DEFAULT);
  SetContentSetting(ContentSettingsPattern::FromURL(url),
                    content_type,
                    resource_identifier,
                    setting);
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.

  PrefService* prefs = NULL;
  DictionaryValue* all_settings_dictionary = NULL;
  HostContentSettings* map_to_modify = &off_the_record_settings_;

  if (!is_off_the_record_) {
    prefs = profile_->GetPrefs();
    all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);
    map_to_modify = &host_content_settings_;
  }

  {
    base::AutoLock auto_lock(lock_);
    for (HostContentSettings::iterator i(map_to_modify->begin());
         i != map_to_modify->end(); ) {
      if (RequiresResourceIdentifier(content_type) ||
          i->second.content_settings.settings[content_type] !=
          CONTENT_SETTING_DEFAULT) {
        if (RequiresResourceIdentifier(content_type))
          i->second.content_settings_for_resources.clear();
        i->second.content_settings.settings[content_type] =
            CONTENT_SETTING_DEFAULT;
        std::string host(i->first);
        if (AllDefault(i->second)) {
          if (all_settings_dictionary)
            all_settings_dictionary->
                RemoveWithoutPathExpansion(host, NULL);
          map_to_modify->erase(i++);
        } else if (all_settings_dictionary) {
          DictionaryValue* host_settings_dictionary;
          bool found =
              all_settings_dictionary->GetDictionaryWithoutPathExpansion(
                  host, &host_settings_dictionary);
          DCHECK(found);
          host_settings_dictionary->RemoveWithoutPathExpansion(
              kTypeNames[content_type], NULL);
          ++i;
        }
      } else {
        ++i;
      }
    }
  }

  updating_preferences_ = true;
  if (!is_off_the_record_)
    ScopedPrefUpdate update(prefs, prefs::kContentSettingsPatterns);
  updating_preferences_ = false;

  NotifyObservers(
      ContentSettingsDetails(ContentSettingsPattern(), content_type, ""));
}

bool HostContentSettingsMap::RequiresResourceIdentifier(
    ContentSettingsType content_type) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableResourceContentSettings)) {
    return kRequiresResourceIdentifier[content_type];
  } else {
    return false;
  }
}

void HostContentSettingsMap::SetBlockThirdPartyCookies(bool block) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This setting may not be directly modified for OTR sessions.  Instead, it
  // is synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  // If the preference block-third-party-cookies is managed then do not allow to
  // change it.
  if (prefs->IsManagedPreference(prefs::kBlockThirdPartyCookies)) {
    NOTREACHED();
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    block_third_party_cookies_ = block;
  }

  if (block)
    prefs->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  else
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
}

void HostContentSettingsMap::SetBlockNonsandboxedPlugins(bool block) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This setting may not be directly modified for OTR sessions.  Instead, it
  // is synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    block_nonsandboxed_plugins_ = block;
  }


  PrefService* prefs = profile_->GetPrefs();
  if (block) {
    UserMetrics::RecordAction(
        UserMetricsAction("BlockNonsandboxedPlugins_Enable"));
    prefs->SetBoolean(prefs::kBlockNonsandboxedPlugins, true);
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("BlockNonsandboxedPlugins_Disable"));
    prefs->ClearPref(prefs::kBlockNonsandboxedPlugins);
  }
}

void HostContentSettingsMap::ResetToDefaults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);
    for (provider_iterator provider = content_settings_providers_.begin();
         provider != content_settings_providers_.end(); ++provider) {
      (*provider)->ResetToDefaults();
    }
    host_content_settings_.clear();
    off_the_record_settings_.clear();
    // Don't reset block third party cookies if they are managed.
    if (!IsBlockThirdPartyCookiesManaged())
      block_third_party_cookies_ = false;
    block_nonsandboxed_plugins_ = false;
  }

  if (!is_off_the_record_) {
    PrefService* prefs = profile_->GetPrefs();
    updating_preferences_ = true;
    prefs->ClearPref(prefs::kContentSettingsPatterns);
    // If the block third party cookies preference is managed we still must
    // clear it in order to restore the default value for later when the
    // preference is not managed anymore.
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
    prefs->ClearPref(prefs::kBlockNonsandboxedPlugins);
    updating_preferences_ = false;
    NotifyObservers(ContentSettingsDetails(ContentSettingsPattern(),
                                           CONTENT_SETTINGS_TYPE_DEFAULT,
                                           ""));
  }
}

void HostContentSettingsMap::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kContentSettingsPatterns) {
      ReadExceptions(true);
    } else if (*name == prefs::kBlockThirdPartyCookies) {
      base::AutoLock auto_lock(lock_);
      block_third_party_cookies_ = profile_->GetPrefs()->GetBoolean(
          prefs::kBlockThirdPartyCookies);
      is_block_third_party_cookies_managed_ =
          profile_->GetPrefs()->IsManagedPreference(
              prefs::kBlockThirdPartyCookies);
    } else if (*name == prefs::kBlockNonsandboxedPlugins) {
      base::AutoLock auto_lock(lock_);
      block_nonsandboxed_plugins_ = profile_->GetPrefs()->GetBoolean(
          prefs::kBlockNonsandboxedPlugins);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    if (!is_off_the_record_) {
      NotifyObservers(ContentSettingsDetails(ContentSettingsPattern(),
                                             CONTENT_SETTINGS_TYPE_DEFAULT,
                                             ""));
    }
  } else if (type == NotificationType::PROFILE_DESTROYED) {
    DCHECK_EQ(profile_, Source<Profile>(source).ptr());
    UnregisterObservers();
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

HostContentSettingsMap::~HostContentSettingsMap() {
  UnregisterObservers();
}

void HostContentSettingsMap::GetSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ContentSettings* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::string& content_type(*i);
    for (size_t type = 0; type < arraysize(kTypeNames); ++type) {
      if ((kTypeNames[type] != NULL) && (kTypeNames[type] == content_type)) {
        int setting = CONTENT_SETTING_DEFAULT;
        bool found = dictionary->GetIntegerWithoutPathExpansion(content_type,
                                                                &setting);
        DCHECK(found);
        settings->settings[type] = IntToContentSetting(setting);
        break;
      }
    }
  }
  // Migrate obsolete cookie prompt mode.
  if (settings->settings[CONTENT_SETTINGS_TYPE_COOKIES] ==
      CONTENT_SETTING_ASK)
    settings->settings[CONTENT_SETTINGS_TYPE_COOKIES] = CONTENT_SETTING_BLOCK;

  settings->settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      ClickToPlayFixup(CONTENT_SETTINGS_TYPE_PLUGINS,
                       settings->settings[CONTENT_SETTINGS_TYPE_PLUGINS]);
}

void HostContentSettingsMap::GetResourceSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ResourceContentSettings* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::string& content_type(*i);
    for (size_t type = 0; type < arraysize(kResourceTypeNames); ++type) {
      if ((kResourceTypeNames[type] != NULL) &&
          (kResourceTypeNames[type] == content_type)) {
        DictionaryValue* resource_dictionary = NULL;
        bool found = dictionary->GetDictionary(content_type,
                                               &resource_dictionary);
        DCHECK(found);
        for (DictionaryValue::key_iterator j(resource_dictionary->begin_keys());
             j != resource_dictionary->end_keys(); ++j) {
          const std::string& resource_identifier(*j);
          int setting = CONTENT_SETTING_DEFAULT;
          bool found = resource_dictionary->GetIntegerWithoutPathExpansion(
              resource_identifier, &setting);
          DCHECK(found);
          (*settings)[ContentSettingsTypeResourceIdentifierPair(
              ContentSettingsType(type), resource_identifier)] =
                  ClickToPlayFixup(ContentSettingsType(type),
                                   ContentSetting(setting));
        }

        break;
      }
    }
  }
}

bool HostContentSettingsMap::AllDefault(
    const ExtendedContentSettings& settings) const {
  for (size_t i = 0; i < arraysize(settings.content_settings.settings); ++i) {
    if (settings.content_settings.settings[i] != CONTENT_SETTING_DEFAULT)
      return false;
  }
  return settings.content_settings_for_resources.empty();
}

bool HostContentSettingsMap::IsDefaultContentSettingManaged(
    ContentSettingsType content_type) const {
  for (const_provider_iterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end(); ++provider) {
    if ((*provider)->DefaultSettingIsManaged(content_type))
      return true;
  }
  return false;
}

void HostContentSettingsMap::ReadExceptions(bool overwrite) {
  base::AutoLock lock(lock_);

  PrefService* prefs = profile_->GetPrefs();
  DictionaryValue* all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);

  if (overwrite)
    host_content_settings_.clear();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    // Convert all Unicode patterns into punycode form, then read.
    CanonicalizeContentSettingsExceptions(all_settings_dictionary);

    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::string& pattern(*i);
      if (!ContentSettingsPattern(pattern).IsValid())
        LOG(WARNING) << "Invalid pattern stored in content settings";
      DictionaryValue* pattern_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          pattern, &pattern_settings_dictionary);
      DCHECK(found);
      ContentSettings settings;
      GetSettingsFromDictionary(pattern_settings_dictionary, &settings);
      host_content_settings_[pattern].content_settings = settings;
      GetResourceSettingsFromDictionary(
          pattern_settings_dictionary,
          &host_content_settings_[pattern].content_settings_for_resources);
    }
  }
}

void HostContentSettingsMap::NotifyObservers(
    const ContentSettingsDetails& details) {
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(this),
      Details<const ContentSettingsDetails>(&details));
}

void HostContentSettingsMap::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  pref_change_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}

void HostContentSettingsMap::MigrateObsoleteCookiePref(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kCookieBehavior)) {
    int cookie_behavior = prefs->GetInteger(prefs::kCookieBehavior);
    prefs->ClearPref(prefs::kCookieBehavior);
    if (!prefs->HasPrefPath(prefs::kDefaultContentSettings)) {
        SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
            (cookie_behavior == net::StaticCookiePolicy::BLOCK_ALL_COOKIES) ?
                CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
    }
    if (!prefs->HasPrefPath(prefs::kBlockThirdPartyCookies)) {
      SetBlockThirdPartyCookies(cookie_behavior ==
          net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    }
  }
}

void HostContentSettingsMap::MigrateObsoletePopupsPref(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kPopupWhitelistedHosts)) {
    const ListValue* whitelist_pref =
        prefs->GetList(prefs::kPopupWhitelistedHosts);
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      SetContentSetting(ContentSettingsPattern(host),
                        CONTENT_SETTINGS_TYPE_POPUPS,
                        "",
                        CONTENT_SETTING_ALLOW);
    }
    prefs->ClearPref(prefs::kPopupWhitelistedHosts);
  }
}

void HostContentSettingsMap::MigrateObsoletePerhostPref(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kPerHostContentSettings)) {
    const DictionaryValue* all_settings_dictionary =
        prefs->GetDictionary(prefs::kPerHostContentSettings);
    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::string& host(*i);
      ContentSettingsPattern pattern(
          std::string(ContentSettingsPattern::kDomainWildcard) + host);
      DictionaryValue* host_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          host, &host_settings_dictionary);
      DCHECK(found);
      ContentSettings settings;
      GetSettingsFromDictionary(host_settings_dictionary, &settings);
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
        if (settings.settings[j] != CONTENT_SETTING_DEFAULT &&
            !RequiresResourceIdentifier(ContentSettingsType(j)))
          SetContentSetting(
              pattern, ContentSettingsType(j), "", settings.settings[j]);
      }
    }
    prefs->ClearPref(prefs::kPerHostContentSettings);
  }
}

void HostContentSettingsMap::CanonicalizeContentSettingsExceptions(
    DictionaryValue* all_settings_dictionary) {
  DCHECK(all_settings_dictionary);

  std::vector<std::string> remove_items;
  std::vector<std::pair<std::string, std::string> > move_items;
  for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
       i != all_settings_dictionary->end_keys(); ++i) {
    const std::string& pattern(*i);
    const std::string canonicalized_pattern =
        ContentSettingsPattern(pattern).CanonicalizePattern();

    if (canonicalized_pattern.empty() || canonicalized_pattern == pattern)
      continue;

    // Clear old pattern if prefs already have canonicalized pattern.
    DictionaryValue* new_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            canonicalized_pattern, &new_pattern_settings_dictionary)) {
      remove_items.push_back(pattern);
      continue;
    }

    // Move old pattern to canonicalized pattern.
    DictionaryValue* old_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            pattern, &old_pattern_settings_dictionary)) {
      move_items.push_back(std::make_pair(pattern, canonicalized_pattern));
    }
  }

  for (size_t i = 0; i < remove_items.size(); ++i) {
    all_settings_dictionary->RemoveWithoutPathExpansion(remove_items[i], NULL);
  }

  for (size_t i = 0; i < move_items.size(); ++i) {
    Value* pattern_settings_dictionary = NULL;
    all_settings_dictionary->RemoveWithoutPathExpansion(
        move_items[i].first, &pattern_settings_dictionary);
    all_settings_dictionary->SetWithoutPathExpansion(
        move_items[i].second, pattern_settings_dictionary);
  }
}
