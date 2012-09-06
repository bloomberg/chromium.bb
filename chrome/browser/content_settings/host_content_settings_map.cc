// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"

#include <utility>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_default_provider.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_custom_extension_provider.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_internal_extension_provider.h"
#include "chrome/browser/content_settings/content_settings_policy_provider.h"
#include "chrome/browser/content_settings/content_settings_pref_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

typedef std::vector<content_settings::Rule> Rules;

typedef std::pair<std::string, std::string> StringPair;

const char* kProviderNames[] = {
  "platform_app",
  "policy",
  "extension",
  "preference",
  "default"
};

content_settings::SettingSource kProviderSourceMap[] = {
  content_settings::SETTING_SOURCE_EXTENSION,
  content_settings::SETTING_SOURCE_POLICY,
  content_settings::SETTING_SOURCE_EXTENSION,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
};
COMPILE_ASSERT(arraysize(kProviderSourceMap) ==
                   HostContentSettingsMap::NUM_PROVIDER_TYPES,
               kProviderSourceMap_has_incorrect_size);

// Returns true if the |content_type| supports a resource identifier.
// Resource identifiers are supported (but not required) for plug-ins.
bool SupportsResourceIdentifier(ContentSettingsType content_type) {
  return content_type == CONTENT_SETTINGS_TYPE_PLUGINS;
}

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(
    PrefService* prefs,
    bool incognito)
    : prefs_(prefs),
      is_off_the_record_(incognito) {
  content_settings::ObservableProvider* policy_provider =
      new content_settings::PolicyProvider(prefs_);
  policy_provider->AddObserver(this);
  content_settings_providers_[POLICY_PROVIDER] = policy_provider;

  content_settings::ObservableProvider* pref_provider =
      new content_settings::PrefProvider(prefs_, is_off_the_record_);
  pref_provider->AddObserver(this);
  content_settings_providers_[PREF_PROVIDER] = pref_provider;

  content_settings::ObservableProvider* default_provider =
      new content_settings::DefaultProvider(prefs_, is_off_the_record_);
  default_provider->AddObserver(this);
  content_settings_providers_[DEFAULT_PROVIDER] = default_provider;

  if (!is_off_the_record_) {
    // Migrate obsolete preferences.
    MigrateObsoleteClearOnExitPref();
  }
}

#if defined(ENABLE_EXTENSIONS)
void HostContentSettingsMap::RegisterExtensionService(
    ExtensionService* extension_service) {
  DCHECK(extension_service);
  DCHECK(!content_settings_providers_[INTERNAL_EXTENSION_PROVIDER]);
  DCHECK(!content_settings_providers_[CUSTOM_EXTENSION_PROVIDER]);

  content_settings::InternalExtensionProvider* internal_extension_provider =
      new content_settings::InternalExtensionProvider(extension_service);
  internal_extension_provider->AddObserver(this);
  content_settings_providers_[INTERNAL_EXTENSION_PROVIDER] =
      internal_extension_provider;

  content_settings::ObservableProvider* custom_extension_provider =
      new content_settings::CustomExtensionProvider(
          extension_service->GetContentSettingsStore(),
          is_off_the_record_);
  custom_extension_provider->AddObserver(this);
  content_settings_providers_[CUSTOM_EXTENSION_PROVIDER] =
      custom_extension_provider;

  OnContentSettingChanged(ContentSettingsPattern(),
                          ContentSettingsPattern(),
                          CONTENT_SETTINGS_TYPE_DEFAULT,
                          "");
}
#endif

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kContentSettingsDefaultWhitelistVersion,
                             0, PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kContentSettingsClearOnExitMigrated,
                             false, PrefService::SYNCABLE_PREF);

  // Register the prefs for the content settings providers.
  content_settings::DefaultProvider::RegisterUserPrefs(prefs);
  content_settings::PrefProvider::RegisterUserPrefs(prefs);
  content_settings::PolicyProvider::RegisterUserPrefs(prefs);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingFromProvider(
    ContentSettingsType content_type,
    content_settings::ProviderInterface* provider) const {
  scoped_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, "", false));

  ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();
  while (rule_iterator->HasNext()) {
    content_settings::Rule rule = rule_iterator->Next();
    if (rule.primary_pattern == wildcard &&
        rule.secondary_pattern == wildcard) {
      return content_settings::ValueToContentSetting(rule.value.get());
    }
  }
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type,
    std::string* provider_id) const {
  // Iterate through the list of providers and return the first non-NULL value
  // that matches |primary_url| and |secondary_url|.
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    if (provider->first == PREF_PROVIDER)
      continue;
    ContentSetting default_setting =
        GetDefaultContentSettingFromProvider(content_type, provider->second);
    if (default_setting != CONTENT_SETTING_DEFAULT) {
      if (provider_id)
        *provider_id = kProviderNames[provider->first];
      return default_setting;
    }
  }

  // The method GetDefaultContentSetting always has to return an explicit
  // value that is to be used as default. We here rely on the
  // DefaultProvider to always provide a value.
  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  DCHECK(!ContentTypeHasCompoundValue(content_type));
  scoped_ptr<base::Value> value(GetWebsiteSetting(
      primary_url, secondary_url, content_type, resource_identifier, NULL));
  return content_settings::ValueToContentSetting(value.get());
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings) const {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  DCHECK(settings);

  settings->clear();
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    // For each provider, iterate first the incognito-specific rules, then the
    // normal rules.
    if (is_off_the_record_) {
      AddSettingsForOneType(provider->second,
                            provider->first,
                            content_type,
                            resource_identifier,
                            settings,
                            true);
    }
    AddSettingsForOneType(provider->second,
                          provider->first,
                          content_type,
                          resource_identifier,
                          settings,
                          false);
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(IsSettingAllowedForType(prefs_, setting, content_type));

  base::Value* value = NULL;
  if (setting != CONTENT_SETTING_DEFAULT)
    value = Value::CreateIntegerValue(setting);
  SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      content_type,
      std::string(),
      value);
}

void HostContentSettingsMap::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    base::Value* value) {
  DCHECK(IsValueAllowedForType(prefs_, value, content_type));
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    if (provider->second->SetWebsiteSetting(primary_pattern,
                                            secondary_pattern,
                                            content_type,
                                            resource_identifier,
                                            value)) {
      return;
    }
  }
  NOTREACHED();
}

void HostContentSettingsMap::SetContentSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK(!ContentTypeHasCompoundValue(content_type));
  base::Value* value = NULL;
  if (setting != CONTENT_SETTING_DEFAULT)
    value = Value::CreateIntegerValue(setting);
  SetWebsiteSetting(primary_pattern,
                    secondary_pattern,
                    content_type,
                    resource_identifier,
                    value);
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
  DCHECK(!ContentTypeHasCompoundValue(content_type));

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

bool HostContentSettingsMap::IsValueAllowedForType(
    PrefService* prefs, const base::Value* value, ContentSettingsType type) {
  return ContentTypeHasCompoundValue(type) || IsSettingAllowedForType(
      prefs, content_settings::ValueToContentSetting(value), type);
}

// static
bool HostContentSettingsMap::IsSettingAllowedForType(
    PrefService* prefs,
    ContentSetting setting,
    ContentSettingsType content_type) {
  // Intents content settings are hidden behind a switch for now.
  if (content_type == CONTENT_SETTINGS_TYPE_INTENTS) {
    if (!web_intents::IsWebIntentsEnabled(prefs))
      return false;
  }

  // We don't yet support stored content settings for mixed scripting.
  if (content_type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT)
    return false;

  // BLOCK semantics are not implemented for fullscreen.
  if (content_type == CONTENT_SETTINGS_TYPE_FULLSCREEN &&
      setting == CONTENT_SETTING_BLOCK) {
    return false;
  }

  // DEFAULT, ALLOW and BLOCK are always allowed.
  if (setting == CONTENT_SETTING_DEFAULT ||
      setting == CONTENT_SETTING_ALLOW ||
      setting == CONTENT_SETTING_BLOCK) {
    return true;
  }
  switch (content_type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return setting == CONTENT_SETTING_SESSION_ONLY;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_INTENTS:
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
    case CONTENT_SETTINGS_TYPE_PPAPI_BROKER:
      return setting == CONTENT_SETTING_ASK;
    default:
      return false;
  }
}

// static
bool HostContentSettingsMap::ContentTypeHasCompoundValue(
    ContentSettingsType type) {
  // Values for content type CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE are
  // of type dictionary/map. Compound types like dictionaries can't be mapped to
  // the type |ContentSetting|.
  return (type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE ||
          type == CONTENT_SETTINGS_TYPE_MEDIASTREAM);
}

void HostContentSettingsMap::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  const ContentSettingsDetails details(primary_pattern,
                                       secondary_pattern,
                                       content_type,
                                       resource_identifier);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
      content::Source<HostContentSettingsMap>(this),
      content::Details<const ContentSettingsDetails>(&details));
}

HostContentSettingsMap::~HostContentSettingsMap() {
  DCHECK(!prefs_);
  STLDeleteValues(&content_settings_providers_);
}

void HostContentSettingsMap::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  prefs_ = NULL;
  for (ProviderIterator it = content_settings_providers_.begin();
       it != content_settings_providers_.end();
       ++it) {
    it->second->ShutdownOnUIThread();
  }
}

void HostContentSettingsMap::MigrateObsoleteClearOnExitPref() {
  // Don't migrate more than once.
  if (prefs_->HasPrefPath(prefs::kContentSettingsClearOnExitMigrated) &&
      prefs_->GetBoolean(prefs::kContentSettingsClearOnExitMigrated)) {
    return;
  }

  if (!prefs_->GetBoolean(prefs::kClearSiteDataOnExit)) {
    // Nothing to be done
    prefs_->SetBoolean(prefs::kContentSettingsClearOnExitMigrated, true);
    return;
  }

  // Change the default cookie settings:
  //  old              new
  //  ---------------- ----------------
  //  ALLOW            SESSION_ONLY
  //  SESSION_ONLY     SESSION_ONLY
  //  BLOCK            BLOCK
  ContentSetting default_setting = GetDefaultContentSettingFromProvider(
      CONTENT_SETTINGS_TYPE_COOKIES,
      content_settings_providers_[DEFAULT_PROVIDER]);
  if (default_setting == CONTENT_SETTING_ALLOW) {
    SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_SESSION_ONLY);
  }

  // Change the exceptions using the same rules.
  ContentSettingsForOneType exceptions;
  AddSettingsForOneType(content_settings_providers_[PREF_PROVIDER],
                        PREF_PROVIDER,
                        CONTENT_SETTINGS_TYPE_COOKIES,
                        "",
                        &exceptions,
                        false);
  for (ContentSettingsForOneType::iterator it = exceptions.begin();
       it != exceptions.end(); ++it) {
    if (it->setting != CONTENT_SETTING_ALLOW)
      continue;
    SetWebsiteSetting(
        it->primary_pattern,
        it->secondary_pattern,
        CONTENT_SETTINGS_TYPE_COOKIES,
        "",
        Value::CreateIntegerValue(CONTENT_SETTING_SESSION_ONLY));
  }

  prefs_->SetBoolean(prefs::kContentSettingsClearOnExitMigrated, true);
}


void HostContentSettingsMap::AddSettingsForOneType(
    const content_settings::ProviderInterface* provider,
    ProviderType provider_type,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings,
    bool incognito) const {
  scoped_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type,
                                resource_identifier,
                                incognito));
  while (rule_iterator->HasNext()) {
    const content_settings::Rule& rule = rule_iterator->Next();
    ContentSetting setting_value = CONTENT_SETTING_DEFAULT;
    // TODO(bauerb): Return rules as a list of values, not content settings.
    // Handle the case using compound values for its exceptions and arbitrary
    // values for its default setting. Here we assume all the exceptions
    // are granted as |CONTENT_SETTING_ALLOW|.
    if (ContentTypeHasCompoundValue(content_type) &&
        rule.value.get() &&
        rule.primary_pattern != ContentSettingsPattern::Wildcard()) {
      setting_value = CONTENT_SETTING_ALLOW;
    } else {
      setting_value = content_settings::ValueToContentSetting(rule.value.get());
    }
    settings->push_back(ContentSettingPatternSource(
        rule.primary_pattern, rule.secondary_pattern,
        setting_value,
        kProviderNames[provider_type],
        incognito));
  }
}

bool HostContentSettingsMap::ShouldAllowAllContent(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
      content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return false;
  }
  if (secondary_url.SchemeIs(chrome::kChromeUIScheme) &&
      content_type == CONTENT_SETTINGS_TYPE_COOKIES &&
      primary_url.SchemeIsSecure()) {
    return true;
  }
  if (primary_url.SchemeIs(chrome::kExtensionScheme)) {
    return content_type != CONTENT_SETTINGS_TYPE_PLUGINS &&
        (content_type != CONTENT_SETTINGS_TYPE_COOKIES ||
            secondary_url.SchemeIs(chrome::kExtensionScheme));
  }
  return primary_url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         primary_url.SchemeIs(chrome::kChromeInternalScheme) ||
         primary_url.SchemeIs(chrome::kChromeUIScheme);
}

base::Value* HostContentSettingsMap::GetWebsiteSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    content_settings::SettingInfo* info) const {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());

  // Check if the scheme of the requesting url is whitelisted.
  if (ShouldAllowAllContent(primary_url, secondary_url, content_type)) {
    if (info) {
      info->source = content_settings::SETTING_SOURCE_WHITELIST;
      info->primary_pattern = ContentSettingsPattern::Wildcard();
      info->secondary_pattern = ContentSettingsPattern::Wildcard();
    }
    return Value::CreateIntegerValue(CONTENT_SETTING_ALLOW);
  }

  ContentSettingsPattern* primary_pattern = NULL;
  ContentSettingsPattern* secondary_pattern = NULL;
  if (info) {
    primary_pattern = &info->primary_pattern;
    secondary_pattern = &info->secondary_pattern;
  }

  // The list of |content_settings_providers_| is ordered according to their
  // precedence.
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    base::Value* value = content_settings::GetContentSettingValueAndPatterns(
        provider->second, primary_url, secondary_url, content_type,
        resource_identifier, is_off_the_record_,
        primary_pattern, secondary_pattern);
    if (value) {
      if (info)
        info->source = kProviderSourceMap[provider->first];
      return value;
    }
  }

  if (info) {
    info->source = content_settings::SETTING_SOURCE_NONE;
    info->primary_pattern = ContentSettingsPattern();
    info->secondary_pattern = ContentSettingsPattern();
  }
  return NULL;
}
