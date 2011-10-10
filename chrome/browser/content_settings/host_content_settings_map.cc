// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"

#include <list>

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_default_provider.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_extension_provider.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_policy_provider.h"
#include "chrome/browser/content_settings/content_settings_pref_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/user_metrics.h"
#include "content/common/content_switches.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/static_cookie_policy.h"

namespace {

// Returns true if we should allow all content types for this URL.  This is
// true for various internal objects like chrome:// URLs, so UI and other
// things users think of as "not webpages" don't break.
static bool ShouldAllowAllContent(const GURL& url,
                                  ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return false;
  return url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme);
}

typedef linked_ptr<content_settings::ProviderInterface> ProviderPtr;
typedef std::map<HostContentSettingsMap::ProviderType, ProviderPtr>
    ProviderMap;
typedef ProviderMap::iterator ProviderIterator;
typedef ProviderMap::const_iterator ConstProviderIterator;

typedef std::vector<content_settings::Rule> Rules;

typedef std::pair<std::string, std::string> StringPair;

const char* kProviderNames[] = {
  "policy",
  "extension",
  "preference",
  "default"
};

bool ContentTypeHasCompoundValue(ContentSettingsType type) {
  // Values for content type CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE are
  // of type dictionary/map. Compound types like dictionaries can't be mapped to
  // the type |ContentSetting|.
  return type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE;
}

ContentSetting GetDefaultSetting(Rules rules) {
  for (Rules::iterator rule = rules.begin();
       rule != rules.end();
       ++rule) {
    if (rule->primary_pattern == ContentSettingsPattern::Wildcard() &&
        rule->secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return rule->content_setting;
    }
  }
  return CONTENT_SETTING_DEFAULT;
}

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(
    PrefService* prefs,
    ExtensionService* extension_service,
    bool incognito)
    : prefs_(prefs),
      is_off_the_record_(incognito),
      updating_preferences_(false),
      block_third_party_cookies_(false),
      is_block_third_party_cookies_managed_(false) {
  content_settings::ObservableProvider* policy_provider =
      new content_settings::PolicyProvider(prefs_);
  policy_provider->AddObserver(this);
  content_settings_providers_[POLICY_PROVIDER] =
      make_linked_ptr(policy_provider);

  if (extension_service) {
    // |extension_service| can be NULL in unit tests.
    content_settings::ObservableProvider* extension_provider =
        new content_settings::ExtensionProvider(
            extension_service->GetExtensionContentSettingsStore(),
            is_off_the_record_);
    extension_provider->AddObserver(this);
    content_settings_providers_[EXTENSION_PROVIDER] =
        make_linked_ptr(extension_provider);
  }

  content_settings::ObservableProvider* pref_provider =
      new content_settings::PrefProvider(prefs_, is_off_the_record_);
  pref_provider->AddObserver(this);
  content_settings_providers_[PREF_PROVIDER] =
      make_linked_ptr(pref_provider);

  content_settings::ObservableProvider* default_provider =
      new content_settings::DefaultProvider(prefs_, is_off_the_record_);
  default_provider->AddObserver(this);
  content_settings_providers_[DEFAULT_PROVIDER] =
      make_linked_ptr(default_provider);

  MigrateObsoleteCookiePref();

  // Read misc. global settings.
  block_third_party_cookies_ =
      prefs_->GetBoolean(prefs::kBlockThirdPartyCookies);
  if (block_third_party_cookies_) {
    UserMetrics::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingEnabled"));
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingDisabled"));
  }
  is_block_third_party_cookies_managed_ =
      prefs_->IsManagedPreference(prefs::kBlockThirdPartyCookies);

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kBlockThirdPartyCookies, this);
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBlockThirdPartyCookies,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex,
                             0,
                             PrefService::UNSYNCABLE_PREF);

  // Obsolete prefs, for migration:
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::StaticCookiePolicy::ALLOW_ALL_COOKIES,
                             PrefService::UNSYNCABLE_PREF);

  // Register the prefs for the content settings providers.
  content_settings::DefaultProvider::RegisterUserPrefs(prefs);
  content_settings::PrefProvider::RegisterUserPrefs(prefs);
  content_settings::PolicyProvider::RegisterUserPrefs(prefs);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type) const {
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
  ContentSetting default_setting = CONTENT_SETTING_DEFAULT;

  // First check if there is a default setting set by policy.
  ProviderPtr provider =
      content_settings_providers_.find(POLICY_PROVIDER)->second;
  Rules rules;
  provider->GetAllContentSettingsRules(content_type, std::string(), &rules);

  default_setting = GetDefaultSetting(rules);
  if (default_setting != CONTENT_SETTING_DEFAULT)
    return default_setting;

  // Get the default setting.
  provider = content_settings_providers_.find(DEFAULT_PROVIDER)->second;
  provider->GetAllContentSettingsRules(content_type, std::string(), &rules);
  default_setting = GetDefaultSetting(rules);

  // The method GetDefaultContentSetting always has to return an explicit
  // value that is to be used as default. We here rely on the
  // PrefContentSettingProvider to always provide a value.
  CHECK_NE(CONTENT_SETTING_DEFAULT, default_setting);
  return default_setting;
}

ContentSettings HostContentSettingsMap::GetDefaultContentSettings() const {
  ContentSettings output(CONTENT_SETTING_DEFAULT);
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (!ContentTypeHasCompoundValue(ContentSettingsType(i)))
      output.settings[i] = GetDefaultContentSetting(ContentSettingsType(i));
  }
  return output;
}

ContentSetting HostContentSettingsMap::GetCookieContentSetting(
    const GURL& url,
    const GURL& first_party_url,
    bool setting_cookie) const {
  if (ShouldAllowAllContent(first_party_url, CONTENT_SETTINGS_TYPE_COOKIES))
    return CONTENT_SETTING_ALLOW;

  // First get any host-specific settings.
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    if (provider->first == DEFAULT_PROVIDER)
      continue;

    setting = provider->second->GetContentSetting(
        url,
        first_party_url,
        CONTENT_SETTINGS_TYPE_COOKIES,
        std::string());
    if (setting != CONTENT_SETTING_DEFAULT)
      break;
  }

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply that rule.
  if (setting == CONTENT_SETTING_DEFAULT && BlockThirdPartyCookies()) {
    bool strict = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kBlockReadingThirdPartyCookies);
    net::StaticCookiePolicy policy(strict ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv;
    if (setting_cookie)
      rv = policy.CanSetCookie(url, first_party_url);
    else
      rv = policy.CanGetCookies(url, first_party_url);
    DCHECK_NE(net::ERR_IO_PENDING, rv);
    if (rv != net::OK)
      setting = CONTENT_SETTING_BLOCK;
  }

  // If no other policy has changed the setting, use the default.
  if (setting == CONTENT_SETTING_DEFAULT)
    setting = GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);

  return setting;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  DCHECK_NE(CONTENT_SETTINGS_TYPE_COOKIES, content_type);
  DCHECK(content_settings::SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());

  if (ShouldAllowAllContent(secondary_url, content_type))
    return CONTENT_SETTING_ALLOW;

  // Iterate through the list of providers and break when the first non default
  // setting is found.
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    ContentSetting provided_setting = provider->second->GetContentSetting(
        primary_url, secondary_url, content_type, resource_identifier);
    if (provided_setting != CONTENT_SETTING_DEFAULT)
      return provided_setting;
  }
  return CONTENT_SETTING_DEFAULT;
}

Value* HostContentSettingsMap::GetContentSettingValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  // Check if the scheme of the requesting url is whitelisted.
  if (ShouldAllowAllContent(secondary_url, content_type))
    return Value::CreateIntegerValue(CONTENT_SETTING_ALLOW);

  // First check if there are specific settings for the |primary_url| and
  // |secondary_url|. The list of |content_settings_providers_| is ordered
  // according to their priority.
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    Value* value = provider->second->GetContentSettingValue(
        primary_url, secondary_url, content_type, resource_identifier);
    if (value)
      return value;
  }

  // If no specific settings were found for the |primary_url|, |secondary_url|
  // pair, then the default value for the given |content_type| should be
  // returned. For CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE the default is
  // 'no filter available'. That's why we return |NULL| for this content type.
  if (content_type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE)
    return NULL;
  return Value::CreateIntegerValue(GetDefaultContentSetting(content_type));
}

ContentSettings HostContentSettingsMap::GetContentSettings(
      const GURL& primary_url,
      const GURL& secondary_url) const {
  ContentSettings output;
  // If we require a resource identifier, set the content settings to default,
  // otherwise make the defaults explicit. Values for content type
  // CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE can't be mapped to the type
  // |ContentSetting|. So we ignore them here.
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    ContentSettingsType type = ContentSettingsType(j);
    if (type == CONTENT_SETTINGS_TYPE_COOKIES) {
      output.settings[j] = GetCookieContentSetting(
          primary_url, secondary_url, false);
    } else if (!ContentTypeHasCompoundValue(type)) {
      output.settings[j] = GetContentSetting(
          primary_url,
          secondary_url,
          ContentSettingsType(j),
          std::string());
    }
  }
  return output;
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    SettingsForOneType* settings) const {
  DCHECK(content_settings::SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  DCHECK(settings);

  settings->clear();
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    Rules rules;
    provider->second->GetAllContentSettingsRules(content_type,
                                         resource_identifier,
                                         &rules);
    // Sort rules according to their primary-secondary pattern string pairs
    // using a map.
    std::map<StringPair, PatternSettingSourceTuple> settings_map;
    for (Rules::iterator rule = rules.begin();
         rule != rules.end();
         ++rule) {
      // Filter out default settings.
      if (rule->primary_pattern == ContentSettingsPattern::Wildcard() &&
          rule->secondary_pattern == ContentSettingsPattern::Wildcard() &&
          (provider->first == POLICY_PROVIDER ||
           provider->first == DEFAULT_PROVIDER)) {
        continue;
      }

      StringPair sort_key(rule->primary_pattern.ToString(),
                          rule->secondary_pattern.ToString());
      settings_map[sort_key] = PatternSettingSourceTuple(
          rule->primary_pattern,
          rule->secondary_pattern,
          rule->content_setting,
          kProviderNames[provider->first]);
    }

    // TODO(markusheintz): Only the rules that are applied should be added.
    for (std::map<StringPair, PatternSettingSourceTuple>::iterator i(
             settings_map.begin());
         i != settings_map.end();
         ++i) {
      settings->push_back(i->second);
    }
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
  DCHECK(IsSettingAllowedForType(setting, content_type));

  content_settings_providers_[DEFAULT_PROVIDER]->SetContentSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      content_type,
      std::string(),
      setting);
}

void HostContentSettingsMap::SetContentSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
  DCHECK(IsSettingAllowedForType(setting, content_type));
  DCHECK(content_settings::SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    provider->second->SetContentSetting(
        primary_pattern,
        secondary_pattern,
        content_type,
        resource_identifier,
        setting);
  }
}

void HostContentSettingsMap::AddExceptionForURL(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  // TODO(markusheintz): Until the UI supports pattern pairs, both urls must
  // match.
  DCHECK(primary_url == secondary_url);

  // Make sure there is no entry that would override the pattern we are about
  // to insert for exactly this URL.
  SetContentSetting(ContentSettingsPattern::FromURLNoWildcard(primary_url),
                    ContentSettingsPattern::Wildcard(),
                    content_type,
                    resource_identifier,
                    CONTENT_SETTING_DEFAULT);

  SetContentSetting(ContentSettingsPattern::FromURL(primary_url),
                    ContentSettingsPattern::Wildcard(),
                    content_type,
                    resource_identifier,
                    setting);
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    provider->second->ClearAllContentSettingsRules(content_type);
  }
}

// static
bool HostContentSettingsMap::IsSettingAllowedForType(
    ContentSetting setting, ContentSettingsType content_type) {
  // Intents content settings are hidden behind a switch for now.
  if (content_type == CONTENT_SETTINGS_TYPE_INTENTS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebIntents))
    return false;

  // DEFAULT, ALLOW and BLOCK are always allowed.
  if (setting == CONTENT_SETTING_DEFAULT ||
      setting == CONTENT_SETTING_ALLOW ||
      setting == CONTENT_SETTING_BLOCK) {
    return true;
  }
  switch (content_type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return (setting == CONTENT_SETTING_SESSION_ONLY);
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return (setting == CONTENT_SETTING_ASK &&
              CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kEnableClickToPlay));
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_INTENTS:
      return (setting == CONTENT_SETTING_ASK);
    default:
      return false;
  }
}

void HostContentSettingsMap::SetBlockThirdPartyCookies(bool block) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);

  // This setting may not be directly modified for OTR sessions.  Instead, it
  // is synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  // If the preference block-third-party-cookies is managed then do not allow to
  // change it.
  if (prefs_->IsManagedPreference(prefs::kBlockThirdPartyCookies)) {
    NOTREACHED();
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    block_third_party_cookies_ = block;
  }

  prefs_->SetBoolean(prefs::kBlockThirdPartyCookies, block);
}

void HostContentSettingsMap::OnContentSettingChanged(
    ContentSettingsPattern primary_pattern,
    ContentSettingsPattern secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  const ContentSettingsDetails details(primary_pattern,
                                       secondary_pattern,
                                       content_type,
                                       resource_identifier);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(this),
      Details<const ContentSettingsDetails>(&details));
}

void HostContentSettingsMap::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK_EQ(prefs_, Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kBlockThirdPartyCookies) {
      base::AutoLock auto_lock(lock_);
      block_third_party_cookies_ = prefs_->GetBoolean(
          prefs::kBlockThirdPartyCookies);
      is_block_third_party_cookies_managed_ =
          prefs_->IsManagedPreference(
              prefs::kBlockThirdPartyCookies);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

HostContentSettingsMap::~HostContentSettingsMap() {
  DCHECK(!prefs_);
}

bool HostContentSettingsMap::IsDefaultContentSettingManaged(
    ContentSettingsType content_type) const {
  Rules rules;
  content_settings_providers_.find(POLICY_PROVIDER)->second->
      GetAllContentSettingsRules(content_type, std::string(), &rules);
  ContentSetting default_setting = GetDefaultSetting(rules);

  if (default_setting != CONTENT_SETTING_DEFAULT)
    return true;
  return false;
}

void HostContentSettingsMap::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
  for (ProviderIterator it = content_settings_providers_.begin();
       it != content_settings_providers_.end();
       ++it) {
    it->second->ShutdownOnUIThread();
  }
}

void HostContentSettingsMap::MigrateObsoleteCookiePref() {
  if (prefs_->HasPrefPath(prefs::kCookieBehavior)) {
    int cookie_behavior = prefs_->GetInteger(prefs::kCookieBehavior);
    prefs_->ClearPref(prefs::kCookieBehavior);
    if (!prefs_->HasPrefPath(prefs::kDefaultContentSettings)) {
        SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
            (cookie_behavior == net::StaticCookiePolicy::BLOCK_ALL_COOKIES) ?
                CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
    }
    if (!prefs_->HasPrefPath(prefs::kBlockThirdPartyCookies)) {
      SetBlockThirdPartyCookies(cookie_behavior ==
          net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    }
  }
}
