// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"

#include <list>

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_extension_provider.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_policy_provider.h"
#include "chrome/browser/content_settings/content_settings_pref_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
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

typedef linked_ptr<content_settings::DefaultProviderInterface>
    DefaultContentSettingsProviderPtr;
typedef std::vector<DefaultContentSettingsProviderPtr>::iterator
    DefaultProviderIterator;
typedef std::vector<DefaultContentSettingsProviderPtr>::const_iterator
    ConstDefaultProviderIterator;

typedef linked_ptr<content_settings::ProviderInterface> ProviderPtr;
typedef std::vector<ProviderPtr>::iterator ProviderIterator;
typedef std::vector<ProviderPtr>::const_iterator ConstProviderIterator;

typedef content_settings::ProviderInterface::Rules Rules;

typedef std::pair<std::string, std::string> StringPair;

const char* kProviderNames[] = {
  "policy",
  "extension",
  "preference"
};

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
  // The order in which the default content settings providers are created is
  // critical, as providers that are further down in the list (i.e. added later)
  // override providers further up.
  content_settings::PrefDefaultProvider* default_provider =
      new content_settings::PrefDefaultProvider(prefs_, is_off_the_record_);
  default_provider->AddObserver(this);
  default_content_settings_providers_.push_back(
      make_linked_ptr(default_provider));

  content_settings::PolicyDefaultProvider* policy_default_provider =
      new content_settings::PolicyDefaultProvider(prefs_);
  policy_default_provider->AddObserver(this);
  default_content_settings_providers_.push_back(
      make_linked_ptr(policy_default_provider));

  // TODO(markusheintz): Discuss whether it is sensible to move migration code
  // to PrefContentSettingsProvider.
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

  // User defined non default content settings are provided by the PrefProvider.
  // The order in which the content settings providers are created is critical,
  // as providers that are further up in the list (i.e. added earlier) override
  // providers further down.
  content_settings::ObservableProvider* provider =
      new content_settings::PolicyProvider(prefs_, policy_default_provider);
  provider->AddObserver(this);
  content_settings_providers_.push_back(make_linked_ptr(provider));

  if (extension_service) {
    // |extension_service| can be NULL in unit tests.
    provider = new content_settings::ExtensionProvider(
        extension_service->GetExtensionContentSettingsStore(),
        is_off_the_record_);
    provider->AddObserver(this);
    content_settings_providers_.push_back(make_linked_ptr(provider));
  }
  provider = new content_settings::PrefProvider(prefs_, is_off_the_record_);
  provider->AddObserver(this);
  content_settings_providers_.push_back(make_linked_ptr(provider));

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
  content_settings::PrefDefaultProvider::RegisterUserPrefs(prefs);
  content_settings::PolicyDefaultProvider::RegisterUserPrefs(prefs);
  content_settings::PrefProvider::RegisterUserPrefs(prefs);
  content_settings::PolicyProvider::RegisterUserPrefs(prefs);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type) const {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  for (ConstDefaultProviderIterator provider =
           default_content_settings_providers_.begin();
       provider != default_content_settings_providers_.end(); ++provider) {
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

ContentSettings HostContentSettingsMap::GetDefaultContentSettings() const {
  ContentSettings output(CONTENT_SETTING_DEFAULT);
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    output.settings[i] = GetDefaultContentSetting(ContentSettingsType(i));
  return output;
}

ContentSetting HostContentSettingsMap::GetCookieContentSetting(
    const GURL& url,
    const GURL& first_party_url,
    bool setting_cookie) const {
  if (ShouldAllowAllContent(first_party_url, CONTENT_SETTINGS_TYPE_COOKIES))
    return CONTENT_SETTING_ALLOW;

  // First get any host-specific settings.
  ContentSetting setting = GetNonDefaultContentSetting(url,
      first_party_url, CONTENT_SETTINGS_TYPE_COOKIES, "");

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
  DCHECK_NE(content_settings::RequiresResourceIdentifier(content_type),
            resource_identifier.empty());
  return GetContentSettingInternal(
      primary_url, secondary_url, content_type, resource_identifier);
}

ContentSetting HostContentSettingsMap::GetContentSettingInternal(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const {
  ContentSetting setting = GetNonDefaultContentSetting(
      primary_url, secondary_url, content_type, resource_identifier);
  if (setting == CONTENT_SETTING_DEFAULT)
    return GetDefaultContentSetting(content_type);
  return setting;
}

ContentSetting HostContentSettingsMap::GetNonDefaultContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const {
  if (ShouldAllowAllContent(secondary_url, content_type))
    return CONTENT_SETTING_ALLOW;

  // Iterate through the list of providers and break when the first non default
  // setting is found.
  ContentSetting provided_setting(CONTENT_SETTING_DEFAULT);
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    provided_setting = (*provider)->GetContentSetting(
        primary_url, secondary_url, content_type, resource_identifier);
    if (provided_setting != CONTENT_SETTING_DEFAULT)
      return provided_setting;
  }
  return provided_setting;
}

ContentSettings HostContentSettingsMap::GetContentSettings(
      const GURL& primary_url,
      const GURL& secondary_url) const {
  ContentSettings output = GetNonDefaultContentSettings(
      primary_url, secondary_url);

  // If we require a resource identifier, set the content settings to default,
  // otherwise make the defaults explicit.
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    // A managed default content setting has the highest priority and hence
    // will overwrite any previously set value.
    if (output.settings[j] == CONTENT_SETTING_DEFAULT &&
        j != CONTENT_SETTINGS_TYPE_PLUGINS) {
      output.settings[j] = GetDefaultContentSetting(ContentSettingsType(j));
    }
  }
  return output;
}

ContentSettings HostContentSettingsMap::GetNonDefaultContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url) const {
  ContentSettings output(CONTENT_SETTING_DEFAULT);
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    output.settings[j] = GetNonDefaultContentSetting(
        primary_url,
        secondary_url,
        ContentSettingsType(j),
        "");
  }
  return output;
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    SettingsForOneType* settings) const {
  DCHECK_NE(content_settings::RequiresResourceIdentifier(content_type),
            resource_identifier.empty());
  DCHECK(settings);

  settings->clear();
  for (size_t i = 0; i < content_settings_providers_.size(); ++i) {
    // Get rules from the content settings provider.
    Rules rules;
    content_settings_providers_[i]->GetAllContentSettingsRules(
        content_type, resource_identifier, &rules);

    // Sort rules according to their primary-secondary pattern string pairs
    // using a map.
    std::map<StringPair, PatternSettingSourceTuple> settings_map;
    for (Rules::iterator rule = rules.begin();
         rule != rules.end();
         ++rule) {
      StringPair sort_key(rule->primary_pattern.ToString(),
                          rule->secondary_pattern.ToString());
      settings_map[sort_key] = PatternSettingSourceTuple(
          rule->primary_pattern,
          rule->secondary_pattern,
          rule->content_setting,
          kProviderNames[i]);
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
  DCHECK(IsSettingAllowedForType(setting, content_type));
  for (DefaultProviderIterator provider =
           default_content_settings_providers_.begin();
       provider != default_content_settings_providers_.end(); ++provider) {
    (*provider)->UpdateDefaultSetting(content_type, setting);
  }
}

void HostContentSettingsMap::SetContentSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK(IsSettingAllowedForType(setting, content_type));
  DCHECK_NE(content_settings::RequiresResourceIdentifier(content_type),
            resource_identifier.empty());
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    (*provider)->SetContentSetting(
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
    (*provider)->ClearAllContentSettingsRules(content_type);
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
  for (ConstDefaultProviderIterator provider =
           default_content_settings_providers_.begin();
       provider != default_content_settings_providers_.end(); ++provider) {
    if ((*provider)->DefaultSettingIsManaged(content_type))
      return true;
  }
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
    (*it)->ShutdownOnUIThread();
  }
  for (DefaultProviderIterator it = default_content_settings_providers_.begin();
       it != default_content_settings_providers_.end();
       ++it) {
    (*it)->ShutdownOnUIThread();
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
