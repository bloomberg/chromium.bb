// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "net/base/dns_util.h"
#include "net/base/net_util.h"
#include "net/base/static_cookie_policy.h"

namespace {
// The version of the pattern format implemented. Version 1 includes the
// following patterns:
//   - [*.]domain.tld (matches domain.tld and all sub-domains)
//   - host (matches an exact hostname)
//   - a.b.c.d (matches an exact IPv4 ip)
//   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
//   - file:///tmp/test.html (a complete URL without a host)
// Version 2 adds a resource identifier for plugins.
// TODO(jochen): update once this feature is no longer behind a flag.
const int kContentSettingsPatternVersion = 1;

// The format of a domain wildcard.
const char kDomainWildcard[] = "[*.]";

// The length of kDomainWildcard (without the trailing '\0')
const size_t kDomainWildcardLength = arraysize(kDomainWildcard) - 1;

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

// The default setting for each content type.
const ContentSetting kDefaultSettings[CONTENT_SETTINGS_NUM_TYPES] = {
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_COOKIES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_IMAGES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_PLUGINS
  CONTENT_SETTING_BLOCK,  // CONTENT_SETTINGS_TYPE_POPUPS
  CONTENT_SETTING_ASK,    // Not used for Geolocation
  CONTENT_SETTING_ASK,    // Not used for Notifications
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
}  // namespace


struct HostContentSettingsMap::ExtendedContentSettings {
  ContentSettings content_settings;
  ResourceContentSettings content_settings_for_resources;
};

// static
HostContentSettingsMap::Pattern HostContentSettingsMap::Pattern::FromURL(
    const GURL& url) {
  return Pattern(!url.has_host() || url.HostIsIPAddress() ?
      net::GetHostOrSpecFromURL(url) :
      std::string(kDomainWildcard) + url.host());
}

// static
HostContentSettingsMap::Pattern
    HostContentSettingsMap::Pattern::FromURLNoWildcard(const GURL& url) {
  return Pattern(net::GetHostOrSpecFromURL(url));
}

bool HostContentSettingsMap::Pattern::IsValid() const {
  if (pattern_.empty())
    return false;

  const std::string host(pattern_.length() > kDomainWildcardLength &&
                         StartsWithASCII(pattern_, kDomainWildcard, false) ?
                         pattern_.substr(kDomainWildcardLength) :
                         pattern_);
  url_canon::CanonHostInfo host_info;
  return host.find('*') == std::string::npos &&
         !net::CanonicalizeHost(host, &host_info).empty();
}

bool HostContentSettingsMap::Pattern::Matches(const GURL& url) const {
  if (!IsValid())
    return false;

  const std::string host(net::GetHostOrSpecFromURL(url));
  if (pattern_.length() < kDomainWildcardLength ||
      !StartsWithASCII(pattern_, kDomainWildcard, false))
    return pattern_ == host;

  const size_t match =
      host.rfind(pattern_.substr(kDomainWildcardLength));

  return (match != std::string::npos) &&
         (match == 0 || host[match - 1] == '.') &&
         (match + pattern_.length() - kDomainWildcardLength == host.length());
}

std::string HostContentSettingsMap::Pattern::CanonicalizePattern() const {
  if (!IsValid()) {
    return "";
  }

  bool starts_with_wildcard = pattern_.length() > kDomainWildcardLength &&
      StartsWithASCII(pattern_, kDomainWildcard, false);

  const std::string host(starts_with_wildcard ?
      pattern_.substr(kDomainWildcardLength) : pattern_);

  std::string canonicalized_pattern =
      starts_with_wildcard ? kDomainWildcard : "";

  url_canon::CanonHostInfo host_info;
  canonicalized_pattern += net::CanonicalizeHost(host, &host_info);

  return canonicalized_pattern;
}

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile),
      block_third_party_cookies_(false),
      is_off_the_record_(profile_->IsOffTheRecord()),
      updating_preferences_(false) {
  PrefService* prefs = profile_->GetPrefs();

  MigrateObsoleteCookiePref(prefs);

  MigrateObsoletePopupsPref(prefs);

  MigrateObsoletePerhostPref(prefs);

  // Read global defaults.
  DCHECK_EQ(arraysize(kTypeNames),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  ReadDefaultSettings(false);

  // Read misc. global settings.
  block_third_party_cookies_ =
      prefs->GetBoolean(prefs::kBlockThirdPartyCookies);
  block_nonsandboxed_plugins_ =
      prefs->GetBoolean(prefs::kBlockNonsandboxedPlugins);

  // Verify preferences version.
  if (!prefs->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs->SetInteger(prefs::kContentSettingsVersion,
                      kContentSettingsPatternVersion);
  }
  if (prefs->GetInteger(prefs::kContentSettingsVersion) >
      kContentSettingsPatternVersion) {
    LOG(ERROR) << "Unknown content settings version in preferences.";
    return;
  }

  // Read exceptions.
  ReadExceptions(false);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kDefaultContentSettings, this);
  pref_change_registrar_.Add(prefs::kContentSettingsPatterns, this);
  pref_change_registrar_.Add(prefs::kBlockThirdPartyCookies, this);
  pref_change_registrar_.Add(prefs::kBlockNonsandboxedPlugins, this);
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings);
  prefs->RegisterIntegerPref(prefs::kContentSettingsVersion,
                             kContentSettingsPatternVersion);
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
  AutoLock auto_lock(lock_);
  return default_content_settings_.settings[content_type];
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  ContentSetting setting = GetNonDefaultContentSetting(url,
                                                       content_type,
                                                       resource_identifier);
  if (setting == CONTENT_SETTING_DEFAULT)
    return GetDefaultContentSetting(content_type);
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

  AutoLock auto_lock(lock_);

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
  for (std::string key = std::string(kDomainWildcard) + host; ; ) {
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

    const size_t next_dot = key.find('.', kDomainWildcardLength);
    if (next_dot == std::string::npos)
      break;
    key.erase(kDomainWildcardLength, next_dot - kDomainWildcardLength + 1);
  }

  return CONTENT_SETTING_DEFAULT;
}

ContentSettings HostContentSettingsMap::GetContentSettings(
    const GURL& url) const {
  ContentSettings output = GetNonDefaultContentSettings(url);

  AutoLock auto_lock(lock_);

  // If we require a resource identifier, set the content settings to default,
  // otherwise make the defaults explicit.
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    if (RequiresResourceIdentifier(ContentSettingsType(j)))
      output.settings[j] = CONTENT_SETTING_DEFAULT;
    else if (output.settings[j] == CONTENT_SETTING_DEFAULT)
      output.settings[j] = default_content_settings_.settings[j];
  }
  return output;
}

ContentSettings HostContentSettingsMap::GetNonDefaultContentSettings(
    const GURL& url) const {
  if (ShouldAllowAllContent(url))
      return ContentSettings(CONTENT_SETTING_ALLOW);

  AutoLock auto_lock(lock_);

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
  for (std::string key = std::string(kDomainWildcard) + host; ; ) {
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
    const size_t next_dot = key.find('.', kDomainWildcardLength);
    if (next_dot == std::string::npos)
      break;
    key.erase(kDomainWildcardLength, next_dot - kDomainWildcardLength + 1);
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

  AutoLock auto_lock(lock_);
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
      settings->push_back(std::make_pair(Pattern(i->first), setting));
    }
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* prefs = profile_->GetPrefs();

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  DictionaryValue* default_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kDefaultContentSettings);
  std::string dictionary_path(kTypeNames[content_type]);
  updating_preferences_ = true;
  {
    AutoLock auto_lock(lock_);
    ScopedPrefUpdate update(prefs, prefs::kDefaultContentSettings);
    if ((setting == CONTENT_SETTING_DEFAULT) ||
        (setting == kDefaultSettings[content_type])) {
      default_content_settings_.settings[content_type] =
          kDefaultSettings[content_type];
      default_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                              NULL);
    } else {
      default_content_settings_.settings[content_type] = setting;
      default_settings_dictionary->SetWithoutPathExpansion(
          dictionary_path, Value::CreateIntegerValue(setting));
    }
  }
  updating_preferences_ = false;

  NotifyObservers(ContentSettingsDetails(Pattern(), content_type, ""));
}

void HostContentSettingsMap::SetContentSetting(
    const Pattern& original_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(RequiresResourceIdentifier(content_type),
            resource_identifier.empty());

  const Pattern pattern(original_pattern.CanonicalizePattern());

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
    AutoLock auto_lock(lock_);
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
  SetContentSetting(Pattern::FromURLNoWildcard(url),
                    content_type,
                    resource_identifier,
                    CONTENT_SETTING_DEFAULT);
  SetContentSetting(Pattern::FromURL(url),
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
    AutoLock auto_lock(lock_);
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

  NotifyObservers(ContentSettingsDetails(Pattern(), content_type, ""));
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
    AutoLock auto_lock(lock_);
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
    AutoLock auto_lock(lock_);
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
    AutoLock auto_lock(lock_);
    default_content_settings_ = ContentSettings();
    ForceDefaultsToBeExplicit();
    host_content_settings_.clear();
    off_the_record_settings_.clear();
    block_third_party_cookies_ = false;
    block_nonsandboxed_plugins_ = false;
  }

  if (!is_off_the_record_) {
    PrefService* prefs = profile_->GetPrefs();
    updating_preferences_ = true;
    prefs->ClearPref(prefs::kDefaultContentSettings);
    prefs->ClearPref(prefs::kContentSettingsPatterns);
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
    prefs->ClearPref(prefs::kBlockNonsandboxedPlugins);
    updating_preferences_ = false;
    NotifyObservers(
        ContentSettingsDetails(Pattern(), CONTENT_SETTINGS_TYPE_DEFAULT, ""));
  }
}

void HostContentSettingsMap::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (NotificationType::PREF_CHANGED == type) {
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (prefs::kDefaultContentSettings == *name) {
      ReadDefaultSettings(true);
    } else if (prefs::kContentSettingsPatterns == *name) {
      ReadExceptions(true);
    } else if (prefs::kBlockThirdPartyCookies == *name) {
      AutoLock auto_lock(lock_);
      block_third_party_cookies_ = profile_->GetPrefs()->GetBoolean(
          prefs::kBlockThirdPartyCookies);
    } else if (prefs::kBlockNonsandboxedPlugins == *name) {
      AutoLock auto_lock(lock_);
      block_nonsandboxed_plugins_ = profile_->GetPrefs()->GetBoolean(
          prefs::kBlockNonsandboxedPlugins);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    if (!is_off_the_record_) {
      NotifyObservers(
          ContentSettingsDetails(Pattern(), CONTENT_SETTINGS_TYPE_DEFAULT, ""));
    }
  } else if (NotificationType::PROFILE_DESTROYED == type) {
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
                  ContentSetting(setting);
        }

        break;
      }
    }
  }
}

void HostContentSettingsMap::ForceDefaultsToBeExplicit() {
  DCHECK_EQ(arraysize(kDefaultSettings),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (default_content_settings_.settings[i] == CONTENT_SETTING_DEFAULT)
      default_content_settings_.settings[i] = kDefaultSettings[i];
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

void HostContentSettingsMap::ReadDefaultSettings(bool overwrite) {
  PrefService* prefs = profile_->GetPrefs();
  const DictionaryValue* default_settings_dictionary =
      prefs->GetDictionary(prefs::kDefaultContentSettings);

  if (overwrite)
    default_content_settings_ = ContentSettings();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (default_settings_dictionary != NULL) {
    GetSettingsFromDictionary(default_settings_dictionary,
                              &default_content_settings_);
  }
  ForceDefaultsToBeExplicit();
}

void HostContentSettingsMap::ReadExceptions(bool overwrite) {
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
      if (!Pattern(pattern).IsValid())
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
          net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
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
      SetContentSetting(Pattern(host), CONTENT_SETTINGS_TYPE_POPUPS, "",
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
      Pattern pattern(std::string(kDomainWildcard) + host);
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
        Pattern(pattern).CanonicalizePattern();

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
