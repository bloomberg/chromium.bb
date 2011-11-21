// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/content_settings/content_settings_extension_provider.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_policy_provider.h"
#include "chrome/browser/content_settings/content_settings_pref_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

using content::BrowserThread;

namespace {

typedef std::vector<content_settings::Rule> Rules;

typedef std::pair<std::string, std::string> StringPair;

const char* kProviderNames[] = {
  "policy",
  "extension",
  "preference",
  "default"
};

content_settings::SettingSource kProviderSourceMap[] = {
  content_settings::SETTING_SOURCE_POLICY,
  content_settings::SETTING_SOURCE_EXTENSION,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
};
COMPILE_ASSERT(arraysize(kProviderSourceMap) ==
                   HostContentSettingsMap::NUM_PROVIDER_TYPES,
               kProviderSourceMap_has_incorrect_size);

bool ContentTypeHasCompoundValue(ContentSettingsType type) {
  // Values for content type CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE are
  // of type dictionary/map. Compound types like dictionaries can't be mapped to
  // the type |ContentSetting|.
  return type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE;
}

ContentSetting GetDefaultSetting(
    content_settings::RuleIterator* rule_iterator) {
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

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(
    PrefService* prefs,
    ExtensionService* extension_service,
    bool incognito)
    : prefs_(prefs),
      is_off_the_record_(incognito) {
  content_settings::ObservableProvider* policy_provider =
      new content_settings::PolicyProvider(prefs_);
  policy_provider->AddObserver(this);
  content_settings_providers_[POLICY_PROVIDER] = policy_provider;

  if (extension_service) {
    // |extension_service| can be NULL in unit tests.
    content_settings::ObservableProvider* extension_provider =
        new content_settings::ExtensionProvider(
            extension_service->GetExtensionContentSettingsStore(),
            is_off_the_record_);
    extension_provider->AddObserver(this);
    content_settings_providers_[EXTENSION_PROVIDER] = extension_provider;
  }

  content_settings::ObservableProvider* pref_provider =
      new content_settings::PrefProvider(prefs_, is_off_the_record_);
  pref_provider->AddObserver(this);
  content_settings_providers_[PREF_PROVIDER] = pref_provider;

  content_settings::ObservableProvider* default_provider =
      new content_settings::DefaultProvider(prefs_, is_off_the_record_);
  default_provider->AddObserver(this);
  content_settings_providers_[DEFAULT_PROVIDER] = default_provider;
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex,
                             0,
                             PrefService::UNSYNCABLE_PREF);

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
  return GetDefaultSetting(rule_iterator.get());
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type,
    std::string* provider_id) const {
  DCHECK(!ContentTypeHasCompoundValue(content_type));

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
  scoped_ptr<base::Value> value(GetWebsiteSetting(
      primary_url, secondary_url, content_type, resource_identifier, NULL));
  return content_settings::ValueToContentSetting(value.get());
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings) const {
  DCHECK(content_settings::SupportsResourceIdentifier(content_type) ||
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
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
  DCHECK(IsSettingAllowedForType(setting, content_type));

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
  DCHECK(IsValueAllowedForType(value, content_type));
  DCHECK(content_settings::SupportsResourceIdentifier(content_type) ||
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
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
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
    const base::Value* value, ContentSettingsType type) {
  return IsSettingAllowedForType(
      content_settings::ValueToContentSetting(value), type);
}

// static
bool HostContentSettingsMap::IsSettingAllowedForType(
    ContentSetting setting, ContentSettingsType content_type) {
  // Intents content settings are hidden behind a switch for now.
  if (content_type == CONTENT_SETTINGS_TYPE_INTENTS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebIntents))
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
      return setting == CONTENT_SETTING_ASK &&
             CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableClickToPlay);
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_INTENTS:
      return setting == CONTENT_SETTING_ASK;
    default:
      return false;
  }
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
  ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();
  while (rule_iterator->HasNext()) {
    const content_settings::Rule& rule = rule_iterator->Next();
    settings->push_back(ContentSettingPatternSource(
        rule.primary_pattern, rule.secondary_pattern,
        content_settings::ValueToContentSetting(rule.value.get()),
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
  if (primary_url.SchemeIs(chrome::kExtensionScheme)) {
    return content_type != CONTENT_SETTINGS_TYPE_COOKIES ||
        secondary_url.SchemeIs(chrome::kExtensionScheme);
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
  DCHECK(content_settings::SupportsResourceIdentifier(content_type) ||
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
