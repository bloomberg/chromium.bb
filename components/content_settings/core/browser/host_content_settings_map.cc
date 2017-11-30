// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/host_content_settings_map.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_policy_provider.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "url/gurl.h"

using content_settings::WebsiteSettingsInfo;
using content_settings::ContentSettingsInfo;

namespace {

typedef std::vector<content_settings::Rule> Rules;

typedef std::pair<std::string, std::string> StringPair;

struct ProviderNamesSourceMapEntry {
  const char* provider_name;
  content_settings::SettingSource provider_source;
};

const HostContentSettingsMap::ProviderType kFirstProvider =
    HostContentSettingsMap::POLICY_PROVIDER;
const HostContentSettingsMap::ProviderType kFirstUserModifiableProvider =
    HostContentSettingsMap::NOTIFICATION_ANDROID_PROVIDER;

constexpr ProviderNamesSourceMapEntry kProviderNamesSourceMap[] = {
    {"policy", content_settings::SETTING_SOURCE_POLICY},
    {"supervised_user", content_settings::SETTING_SOURCE_SUPERVISED},
    {"extension", content_settings::SETTING_SOURCE_EXTENSION},
    {"notification_android", content_settings::SETTING_SOURCE_USER},
    {"preference", content_settings::SETTING_SOURCE_USER},
    {"default", content_settings::SETTING_SOURCE_USER},
    {"tests", content_settings::SETTING_SOURCE_USER},
    {"tests_other", content_settings::SETTING_SOURCE_USER},
};

static_assert(
    arraysize(kProviderNamesSourceMap) ==
        HostContentSettingsMap::NUM_PROVIDER_TYPES,
    "kProviderNamesSourceMap should have NUM_PROVIDER_TYPES elements");

// Ensure that kFirstUserModifiableProvider is actually the highest precedence
// user modifiable provider.
constexpr bool FirstUserModifiableProviderIsHighestPrecedence() {
  for (size_t i = 0; i < kFirstUserModifiableProvider; ++i) {
    if (kProviderNamesSourceMap[i].provider_source ==
        content_settings::SETTING_SOURCE_USER) {
      return false;
    }
  }
  return kProviderNamesSourceMap[kFirstUserModifiableProvider]
             .provider_source == content_settings::SETTING_SOURCE_USER;
}
static_assert(FirstUserModifiableProviderIsHighestPrecedence(),
              "kFirstUserModifiableProvider is not the highest precedence user "
              "modifiable provider.");

// Returns true if the |content_type| supports a resource identifier.
// Resource identifiers are supported (but not required) for plugins.
bool SupportsResourceIdentifier(ContentSettingsType content_type) {
  return content_type == CONTENT_SETTINGS_TYPE_PLUGINS;
}

bool SchemeCanBeWhitelisted(const std::string& scheme) {
  return scheme == content_settings::kChromeDevToolsScheme ||
         scheme == content_settings::kExtensionScheme ||
         scheme == content_settings::kChromeUIScheme;
}

// Handles inheritance of settings from the regular profile into the incognito
// profile.
std::unique_ptr<base::Value> ProcessIncognitoInheritanceBehavior(
    ContentSettingsType content_type,
    std::unique_ptr<base::Value> value) {
  // Website setting inheritance can be completely disallowed.
  const WebsiteSettingsInfo* website_settings_info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          content_type);
  if (website_settings_info &&
      website_settings_info->incognito_behavior() ==
          WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO) {
    return nullptr;
  }

  // Content setting inheritance can be for settings, that are more permissive
  // than the initial value of a content setting.
  const ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  if (content_settings_info) {
    ContentSettingsInfo::IncognitoBehavior behaviour =
        content_settings_info->incognito_behavior();
    switch (behaviour) {
      case ContentSettingsInfo::INHERIT_IN_INCOGNITO:
        return value;
      case ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE:
        ContentSetting setting =
            content_settings::ValueToContentSetting(value.get());
        const base::Value* initial_value = content_settings_info
            ->website_settings_info()->initial_default_value();
        ContentSetting initial_setting =
            content_settings::ValueToContentSetting(initial_value);
        if (content_settings::IsMorePermissive(setting, initial_setting))
          return content_settings::ContentSettingToValue(initial_setting);
        return value;
    }
  }

  return value;
}

content_settings::PatternPair GetPatternsFromScopingType(
    WebsiteSettingsInfo::ScopingType scoping_type,
    const GURL& primary_url,
    const GURL& secondary_url) {
  DCHECK(!primary_url.is_empty());
  content_settings::PatternPair patterns;

  switch (scoping_type) {
    case WebsiteSettingsInfo::REQUESTING_DOMAIN_ONLY_SCOPE:
      patterns.first = ContentSettingsPattern::FromURL(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
    case WebsiteSettingsInfo::TOP_LEVEL_ORIGIN_ONLY_SCOPE:
    case WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE:
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
    case WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE:
      DCHECK(!secondary_url.is_empty());
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second =
          ContentSettingsPattern::FromURLNoWildcard(secondary_url);
      break;
  }
  return patterns;
}

content_settings::PatternPair GetPatternsForContentSettingsType(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) {
  const WebsiteSettingsInfo* website_settings_info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(type);
  DCHECK(website_settings_info);
  content_settings::PatternPair patterns = GetPatternsFromScopingType(
      website_settings_info->scoping_type(), primary_url, secondary_url);
  return patterns;
}

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(PrefService* prefs,
                                               bool is_incognito_profile,
                                               bool is_guest_profile,
                                               bool store_last_modified)
    : RefcountedKeyedService(base::ThreadTaskRunnerHandle::Get()),
#ifndef NDEBUG
      used_from_thread_id_(base::PlatformThread::CurrentId()),
#endif
      prefs_(prefs),
      is_incognito_(is_incognito_profile || is_guest_profile),
      store_last_modified_(store_last_modified),
      weak_ptr_factory_(this) {
  DCHECK(!(is_incognito_profile && is_guest_profile));

  content_settings::PolicyProvider* policy_provider =
      new content_settings::PolicyProvider(prefs_);
  content_settings_providers_[POLICY_PROVIDER] =
      base::WrapUnique(policy_provider);
  policy_provider->AddObserver(this);

  pref_provider_ = new content_settings::PrefProvider(prefs_, is_incognito_,
                                                      store_last_modified_);
  content_settings_providers_[PREF_PROVIDER] = base::WrapUnique(pref_provider_);
  user_modifiable_providers_.push_back(pref_provider_);
  pref_provider_->AddObserver(this);

  // This ensures that content settings are cleared for the guest profile. This
  // wouldn't be needed except that we used to allow settings to be stored for
  // the guest profile and so we need to ensure those get cleared.
  if (is_guest_profile)
    pref_provider_->ClearPrefs();

  auto default_provider = base::MakeUnique<content_settings::DefaultProvider>(
      prefs_, is_incognito_);
  default_provider->AddObserver(this);
  content_settings_providers_[DEFAULT_PROVIDER] = std::move(default_provider);

  RecordExceptionMetrics();
}

// static
void HostContentSettingsMap::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Ensure the content settings are all registered.
  content_settings::ContentSettingsRegistry::GetInstance();

  registry->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex, 0);

  // Register the prefs for the content settings providers.
  content_settings::DefaultProvider::RegisterProfilePrefs(registry);
  content_settings::PrefProvider::RegisterProfilePrefs(registry);
  content_settings::PolicyProvider::RegisterProfilePrefs(registry);
}

void HostContentSettingsMap::RegisterUserModifiableProvider(
    ProviderType type,
    std::unique_ptr<content_settings::UserModifiableProvider> provider) {
  user_modifiable_providers_.push_back(provider.get());
  RegisterProvider(type, std::move(provider));
}

void HostContentSettingsMap::RegisterProvider(
    ProviderType type,
    std::unique_ptr<content_settings::ObservableProvider> provider) {
  DCHECK(!content_settings_providers_[type]);
  provider->AddObserver(this);
  content_settings_providers_[type] = std::move(provider);

#ifndef NDEBUG
  DCHECK_NE(used_from_thread_id_, base::kInvalidThreadId)
      << "Used from multiple threads before initialization complete.";
#endif

  OnContentSettingChanged(ContentSettingsPattern(),
                          ContentSettingsPattern(),
                          CONTENT_SETTINGS_TYPE_DEFAULT,
                          std::string());
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingFromProvider(
    ContentSettingsType content_type,
    content_settings::ProviderInterface* provider) const {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, std::string(), false));

  if (rule_iterator) {
    ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();
    while (rule_iterator->HasNext()) {
      content_settings::Rule rule = rule_iterator->Next();
      if (rule.primary_pattern == wildcard &&
          rule.secondary_pattern == wildcard) {
        return content_settings::ValueToContentSetting(rule.value.get());
      }
    }
  }
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingInternal(
    ContentSettingsType content_type,
    ProviderType* provider_type) const {
  DCHECK(provider_type);
  UsedContentSettingsProviders();

  // Iterate through the list of providers and return the first non-NULL value
  // that matches |primary_url| and |secondary_url|.
  for (const auto& provider_pair : content_settings_providers_) {
    if (provider_pair.first == PREF_PROVIDER)
      continue;
    ContentSetting default_setting = GetDefaultContentSettingFromProvider(
        content_type, provider_pair.second.get());
    if (is_incognito_) {
      default_setting = content_settings::ValueToContentSetting(
          ProcessIncognitoInheritanceBehavior(
              content_type,
              content_settings::ContentSettingToValue(default_setting))
              .get());
    }
    if (default_setting != CONTENT_SETTING_DEFAULT) {
      *provider_type = provider_pair.first;
      return default_setting;
    }
  }

  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type,
    std::string* provider_id) const {
  ProviderType provider_type = NUM_PROVIDER_TYPES;
  ContentSetting content_setting =
      GetDefaultContentSettingInternal(content_type, &provider_type);
  if (content_setting != CONTENT_SETTING_DEFAULT && provider_id)
    *provider_id = kProviderNamesSourceMap[provider_type].provider_name;
  return content_setting;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));
  std::unique_ptr<base::Value> value = GetWebsiteSetting(
      primary_url, secondary_url, content_type, resource_identifier, nullptr);
  return content_settings::ValueToContentSetting(value.get());
}

ContentSetting HostContentSettingsMap::GetUserModifiableContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));
  std::unique_ptr<base::Value> value = GetWebsiteSettingInternal(
      primary_url, secondary_url, content_type, resource_identifier,
      kFirstUserModifiableProvider, nullptr);
  return content_settings::ValueToContentSetting(value.get());
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings) const {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  DCHECK(settings);
  UsedContentSettingsProviders();

  settings->clear();
  for (const auto& provider_pair : content_settings_providers_) {
    // For each provider, iterate first the incognito-specific rules, then the
    // normal rules.
    if (is_incognito_) {
      AddSettingsForOneType(provider_pair.second.get(), provider_pair.first,
                            content_type, resource_identifier, settings, true);
    }
    AddSettingsForOneType(provider_pair.second.get(), provider_pair.first,
                          content_type, resource_identifier, settings, false);
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  std::unique_ptr<base::Value> value;
  // A value of CONTENT_SETTING_DEFAULT implies deleting the content setting.
  if (setting != CONTENT_SETTING_DEFAULT) {
    DCHECK(content_settings::ContentSettingsRegistry::GetInstance()
               ->Get(content_type)
               ->IsDefaultSettingValid(setting));
    value.reset(new base::Value(setting));
  }
  SetWebsiteSettingCustomScope(ContentSettingsPattern::Wildcard(),
                               ContentSettingsPattern::Wildcard(), content_type,
                               std::string(), std::move(value));
}

void HostContentSettingsMap::SetWebsiteSettingDefaultScope(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    std::unique_ptr<base::Value> value) {
  content_settings::PatternPair patterns = GetPatternsForContentSettingsType(
      primary_url, secondary_url, content_type);
  ContentSettingsPattern primary_pattern = patterns.first;
  ContentSettingsPattern secondary_pattern = patterns.second;
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  SetWebsiteSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               resource_identifier, std::move(value));
}

void HostContentSettingsMap::SetWebsiteSettingCustomScope(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    std::unique_ptr<base::Value> value) {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());
  // TODO(crbug.com/731126): Verify that assumptions for notification content
  // settings are met.
  UsedContentSettingsProviders();

  for (const auto& provider_pair : content_settings_providers_) {
    if (provider_pair.second->SetWebsiteSetting(
            primary_pattern, secondary_pattern, content_type,
            resource_identifier, value.get())) {
      // If successful then ownership is passed to the provider.
      ignore_result(value.release());
      return;
    }
  }
  NOTREACHED();
}

bool HostContentSettingsMap::CanSetNarrowestContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) const {
  content_settings::PatternPair patterns =
      GetNarrowestPatterns(primary_url, secondary_url, type);
  return patterns.first.IsValid() && patterns.second.IsValid();
}

void HostContentSettingsMap::SetNarrowestContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type,
    ContentSetting setting) {
  content_settings::PatternPair patterns =
      GetNarrowestPatterns(primary_url, secondary_url, type);

  if (!patterns.first.IsValid() || !patterns.second.IsValid())
    return;

  SetContentSettingCustomScope(patterns.first, patterns.second, type,
                               std::string(), setting);
}

content_settings::PatternPair HostContentSettingsMap::GetNarrowestPatterns (
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) const {
  // Permission settings are specified via rules. There exists always at least
  // one rule for the default setting. Get the rule that currently defines
  // the permission for the given permission |type|. Then test whether the
  // existing rule is more specific than the rule we are about to create. If
  // the existing rule is more specific, than change the existing rule instead
  // of creating a new rule that would be hidden behind the existing rule.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> v = GetWebsiteSettingInternal(
      primary_url, secondary_url, type, std::string(), kFirstProvider, &info);
  if (info.source != content_settings::SETTING_SOURCE_USER) {
    // Return an invalid pattern if the current setting is not a user setting
    // and thus can't be changed.
    return content_settings::PatternPair();
  }

  content_settings::PatternPair patterns = GetPatternsForContentSettingsType(
      primary_url, secondary_url, type);

  ContentSettingsPattern::Relation r1 =
      info.primary_pattern.Compare(patterns.first);
  if (r1 == ContentSettingsPattern::PREDECESSOR) {
    patterns.first = info.primary_pattern;
  } else if (r1 == ContentSettingsPattern::IDENTITY) {
    ContentSettingsPattern::Relation r2 =
        info.secondary_pattern.Compare(patterns.second);
    DCHECK(r2 != ContentSettingsPattern::DISJOINT_ORDER_POST &&
           r2 != ContentSettingsPattern::DISJOINT_ORDER_PRE);
    if (r2 == ContentSettingsPattern::PREDECESSOR)
      patterns.second = info.secondary_pattern;
  }

  return patterns;
}

void HostContentSettingsMap::SetContentSettingCustomScope(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));

  std::unique_ptr<base::Value> value;
  // A value of CONTENT_SETTING_DEFAULT implies deleting the content setting.
  if (setting != CONTENT_SETTING_DEFAULT) {
    DCHECK(content_settings::ContentSettingsRegistry::GetInstance()
               ->Get(content_type)
               ->IsSettingValid(setting));
    value.reset(new base::Value(setting));
  }
  SetWebsiteSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               resource_identifier, std::move(value));
}

void HostContentSettingsMap::SetContentSettingDefaultScope(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  content_settings::PatternPair patterns = GetPatternsForContentSettingsType(
      primary_url, secondary_url, content_type);

  ContentSettingsPattern primary_pattern = patterns.first;
  ContentSettingsPattern secondary_pattern = patterns.second;
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  SetContentSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               resource_identifier, setting);
}

base::WeakPtr<HostContentSettingsMap> HostContentSettingsMap::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HostContentSettingsMap::RecordExceptionMetrics() {
  for (const content_settings::WebsiteSettingsInfo* info :
       *content_settings::WebsiteSettingsRegistry::GetInstance()) {
    ContentSettingsType content_type = info->type();
    const std::string type_name = info->name();

    ContentSettingsForOneType settings;
    GetSettingsForOneType(content_type, std::string(), &settings);
    size_t num_exceptions = 0;
    for (const ContentSettingPatternSource& setting_entry : settings) {
      // Skip default settings.
      if (setting_entry.primary_pattern == ContentSettingsPattern::Wildcard() &&
          setting_entry.secondary_pattern ==
              ContentSettingsPattern::Wildcard()) {
        continue;
      }

      ContentSettingsPattern::SchemeType scheme =
          setting_entry.primary_pattern.GetScheme();
      UMA_HISTOGRAM_ENUMERATION("ContentSettings.ExceptionScheme", scheme,
                                ContentSettingsPattern::SCHEME_MAX);

      if (scheme == ContentSettingsPattern::SCHEME_FILE) {
        UMA_HISTOGRAM_BOOLEAN("ContentSettings.ExceptionSchemeFile.HasPath",
                              setting_entry.primary_pattern.HasPath());
        size_t num_values;
        int histogram_value =
            ContentSettingTypeToHistogramValue(content_type, &num_values);
        if (setting_entry.primary_pattern.HasPath()) {
          UMA_HISTOGRAM_EXACT_LINEAR(
              "ContentSettings.ExceptionSchemeFile.Type.WithPath",
              histogram_value, num_values);
        } else {
          UMA_HISTOGRAM_EXACT_LINEAR(
              "ContentSettings.ExceptionSchemeFile.Type.WithoutPath",
              histogram_value, num_values);
        }
      }

      if (setting_entry.source == "preference")
        ++num_exceptions;
    }

    std::string histogram_name =
        "ContentSettings.Exceptions." + type_name;

    base::HistogramBase* histogram_pointer = base::Histogram::FactoryGet(
        histogram_name, 1, 1000, 30,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(num_exceptions);
  }
}

void HostContentSettingsMap::AddObserver(content_settings::Observer* observer) {
  observers_.AddObserver(observer);
}

void HostContentSettingsMap::RemoveObserver(
    content_settings::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HostContentSettingsMap::FlushLossyWebsiteSettings() {
  prefs_->SchedulePendingLossyWrites();
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  UsedContentSettingsProviders();
  for (const auto& provider_pair : content_settings_providers_)
    provider_pair.second->ClearAllContentSettingsRules(content_type);
  FlushLossyWebsiteSettings();
}

base::Time HostContentSettingsMap::GetSettingLastModifiedDate(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) const {
  base::Time most_recent_time;
  for (auto* provider : user_modifiable_providers_) {
    base::Time time = provider->GetWebsiteSettingLastModified(
        primary_pattern, secondary_pattern, content_type, std::string());
    most_recent_time = std::max(time, most_recent_time);
  }
  return most_recent_time;
}

void HostContentSettingsMap::ClearSettingsForOneTypeWithPredicate(
    ContentSettingsType content_type,
    base::Time begin_time,
    const PatternSourcePredicate& pattern_predicate) {
  if (pattern_predicate.is_null() && begin_time.is_null()) {
    ClearSettingsForOneType(content_type);
    return;
  }
  UsedContentSettingsProviders();
  ContentSettingsForOneType settings;
  GetSettingsForOneType(content_type, std::string(), &settings);
  for (const ContentSettingPatternSource& setting : settings) {
    if (pattern_predicate.is_null() ||
        pattern_predicate.Run(setting.primary_pattern,
                              setting.secondary_pattern)) {
      for (auto* provider : user_modifiable_providers_) {
        base::Time last_modified = provider->GetWebsiteSettingLastModified(
            setting.primary_pattern, setting.secondary_pattern, content_type,
            std::string());
        if (last_modified >= begin_time) {
          provider->SetWebsiteSetting(setting.primary_pattern,
                                      setting.secondary_pattern, content_type,
                                      std::string(), nullptr);
        }
      }
    }
  }
}

void HostContentSettingsMap::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  for (content_settings::Observer& observer : observers_) {
    observer.OnContentSettingChanged(primary_pattern, secondary_pattern,
                                     content_type, resource_identifier);
  }
}

HostContentSettingsMap::~HostContentSettingsMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!prefs_);
}

void HostContentSettingsMap::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  prefs_ = nullptr;
  for (const auto& provider_pair : content_settings_providers_)
    provider_pair.second->ShutdownOnUIThread();
}

void HostContentSettingsMap::AddSettingsForOneType(
    const content_settings::ProviderInterface* provider,
    ProviderType provider_type,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSettingsForOneType* settings,
    bool incognito) const {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, resource_identifier, incognito));
  if (!rule_iterator)
    return;

  while (rule_iterator->HasNext()) {
    const content_settings::Rule& rule = rule_iterator->Next();
    settings->push_back(ContentSettingPatternSource(
        rule.primary_pattern, rule.secondary_pattern,
        base::MakeUnique<base::Value>(rule.value->Clone()),
        kProviderNamesSourceMap[provider_type].provider_name, incognito));
  }
}

void HostContentSettingsMap::UsedContentSettingsProviders() const {
#ifndef NDEBUG
  if (used_from_thread_id_ == base::kInvalidThreadId)
    return;

  if (base::PlatformThread::CurrentId() != used_from_thread_id_)
    used_from_thread_id_ = base::kInvalidThreadId;
#endif
}

std::unique_ptr<base::Value> HostContentSettingsMap::GetWebsiteSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    content_settings::SettingInfo* info) const {
  DCHECK(SupportsResourceIdentifier(content_type) ||
         resource_identifier.empty());

  // Check if the requested setting is whitelisted.
  // TODO(raymes): Move this into GetContentSetting. This has nothing to do with
  // website settings
  const content_settings::ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  if (content_settings_info) {
    for (const std::string& scheme :
         content_settings_info->whitelisted_schemes()) {
      DCHECK(SchemeCanBeWhitelisted(scheme));

      if (primary_url.SchemeIs(scheme)) {
        if (info) {
          info->source = content_settings::SETTING_SOURCE_WHITELIST;
          info->primary_pattern = ContentSettingsPattern::Wildcard();
          info->secondary_pattern = ContentSettingsPattern::Wildcard();
        }
        return std::unique_ptr<base::Value>(
            new base::Value(CONTENT_SETTING_ALLOW));
      }
    }
  }

  return GetWebsiteSettingInternal(primary_url, secondary_url, content_type,
                                   resource_identifier, kFirstProvider, info);
}

// static
HostContentSettingsMap::ProviderType
HostContentSettingsMap::GetProviderTypeFromSource(const std::string& source) {
  for (size_t i = 0; i < arraysize(kProviderNamesSourceMap); ++i) {
    if (source == kProviderNamesSourceMap[i].provider_name)
      return static_cast<ProviderType>(i);
  }

  NOTREACHED();
  return DEFAULT_PROVIDER;
}

std::unique_ptr<base::Value> HostContentSettingsMap::GetWebsiteSettingInternal(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ProviderType first_provider_to_search,
    content_settings::SettingInfo* info) const {
  UsedContentSettingsProviders();
  ContentSettingsPattern* primary_pattern = nullptr;
  ContentSettingsPattern* secondary_pattern = nullptr;
  if (info) {
    primary_pattern = &info->primary_pattern;
    secondary_pattern = &info->secondary_pattern;
  }

  // The list of |content_settings_providers_| is ordered according to their
  // precedence.
  auto it = content_settings_providers_.lower_bound(first_provider_to_search);
  for (; it != content_settings_providers_.end(); ++it) {
    std::unique_ptr<base::Value> value = GetContentSettingValueAndPatterns(
        it->second.get(), primary_url, secondary_url, content_type,
        resource_identifier, is_incognito_, primary_pattern, secondary_pattern);
    if (value) {
      if (info)
        info->source = kProviderNamesSourceMap[it->first].provider_source;
      return value;
    }
  }

  if (info) {
    info->source = content_settings::SETTING_SOURCE_NONE;
    info->primary_pattern = ContentSettingsPattern();
    info->secondary_pattern = ContentSettingsPattern();
  }
  return std::unique_ptr<base::Value>();
}

// static
std::unique_ptr<base::Value>
HostContentSettingsMap::GetContentSettingValueAndPatterns(
    const content_settings::ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  if (include_incognito) {
    // Check incognito-only specific settings. It's essential that the
    // |RuleIterator| gets out of scope before we get a rule iterator for the
    // normal mode.
    std::unique_ptr<content_settings::RuleIterator> incognito_rule_iterator(
        provider->GetRuleIterator(content_type, resource_identifier,
                                  true /* incognito */));
    std::unique_ptr<base::Value> value = GetContentSettingValueAndPatterns(
        incognito_rule_iterator.get(), primary_url, secondary_url,
        primary_pattern, secondary_pattern);
    if (value)
      return value;
  }
  // No settings from the incognito; use the normal mode.
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, resource_identifier,
                                false /* incognito */));
  std::unique_ptr<base::Value> value = GetContentSettingValueAndPatterns(
      rule_iterator.get(), primary_url, secondary_url, primary_pattern,
      secondary_pattern);
  if (value && include_incognito)
    value = ProcessIncognitoInheritanceBehavior(content_type, std::move(value));
  return value;
}

// static
std::unique_ptr<base::Value>
HostContentSettingsMap::GetContentSettingValueAndPatterns(
    content_settings::RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  if (rule_iterator) {
    while (rule_iterator->HasNext()) {
      const content_settings::Rule& rule = rule_iterator->Next();
      if (rule.primary_pattern.Matches(primary_url) &&
          rule.secondary_pattern.Matches(secondary_url)) {
        if (primary_pattern)
          *primary_pattern = rule.primary_pattern;
        if (secondary_pattern)
          *secondary_pattern = rule.secondary_pattern;
        return base::WrapUnique(rule.value.get()->DeepCopy());
      }
    }
  }
  return std::unique_ptr<base::Value>();
}

void HostContentSettingsMap::SetClockForTesting(base::Clock* clock) {
  pref_provider_->SetClockForTesting(clock);
}
