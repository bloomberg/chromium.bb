// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_registry.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "url/gurl.h"

namespace {

const char kPatternSeparator[] = ",";

}  // namespace

namespace content_settings {

std::string ContentSettingToString(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return "allow";
    case CONTENT_SETTING_ASK:
      return "ask";
    case CONTENT_SETTING_BLOCK:
      return "block";
    case CONTENT_SETTING_SESSION_ONLY:
      return "session";
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
      return "detect";
    case CONTENT_SETTING_DEFAULT:
      return "default";
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }

  return ResourceIdentifier();
}

ContentSetting ContentSettingFromString(const std::string& name) {
  if (name == "allow")
    return CONTENT_SETTING_ALLOW;
  if (name == "ask")
    return CONTENT_SETTING_ASK;
  if (name == "block")
    return CONTENT_SETTING_BLOCK;
  if (name == "session")
    return CONTENT_SETTING_SESSION_ONLY;
  if (name == "detect")
    return CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;

  NOTREACHED() << name << " is not a recognized content setting.";
  return CONTENT_SETTING_DEFAULT;
}

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern) {
  return item_pattern.ToString()
         + std::string(kPatternSeparator)
         + top_level_frame_pattern.ToString();
}

PatternPair ParsePatternString(const std::string& pattern_str) {
  std::vector<std::string> pattern_str_list = base::SplitString(
      pattern_str, std::string(1, kPatternSeparator[0]),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // If the |pattern_str| is an empty string then the |pattern_string_list|
  // contains a single empty string. In this case the empty string will be
  // removed to signal an invalid |pattern_str|. Invalid pattern strings are
  // handle by the "if"-statment below. So the order of the if statements here
  // must be preserved.
  if (pattern_str_list.size() == 1) {
    if (pattern_str_list[0].empty()) {
      pattern_str_list.pop_back();
    } else {
      pattern_str_list.push_back("*");
    }
  }

  if (pattern_str_list.size() > 2 ||
      pattern_str_list.size() == 0) {
    return PatternPair(ContentSettingsPattern(),
                       ContentSettingsPattern());
  }

  PatternPair pattern_pair;
  pattern_pair.first =
      ContentSettingsPattern::FromString(pattern_str_list[0]);
  pattern_pair.second =
      ContentSettingsPattern::FromString(pattern_str_list[1]);
  return pattern_pair;
}

ContentSetting ValueToContentSetting(const base::Value* value) {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  bool valid = ParseContentSettingValue(value, &setting);
  DCHECK(valid);
  return setting;
}

bool ParseContentSettingValue(const base::Value* value,
                              ContentSetting* setting) {
  if (!value) {
    *setting = CONTENT_SETTING_DEFAULT;
    return true;
  }
  int int_value = -1;
  if (!value->GetAsInteger(&int_value))
    return false;
  *setting = IntToContentSetting(int_value);
  return *setting != CONTENT_SETTING_DEFAULT;
}

scoped_ptr<base::Value> ContentSettingToValue(ContentSetting setting) {
  if (setting <= CONTENT_SETTING_DEFAULT ||
      setting >= CONTENT_SETTING_NUM_SETTINGS) {
    return nullptr;
  }
  return make_scoped_ptr(new base::FundamentalValue(setting));
}

base::Value* GetContentSettingValueAndPatterns(
    const ProviderInterface* provider,
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
    scoped_ptr<RuleIterator> incognito_rule_iterator(
        provider->GetRuleIterator(content_type, resource_identifier, true));
    base::Value* value = GetContentSettingValueAndPatterns(
        incognito_rule_iterator.get(), primary_url, secondary_url,
        primary_pattern, secondary_pattern);
    if (value)
      return value;
  }
  // No settings from the incognito; use the normal mode.
  scoped_ptr<RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, resource_identifier, false));
  return GetContentSettingValueAndPatterns(
      rule_iterator.get(), primary_url, secondary_url,
      primary_pattern, secondary_pattern);
}

base::Value* GetContentSettingValueAndPatterns(
    RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  while (rule_iterator->HasNext()) {
    const Rule& rule = rule_iterator->Next();
    if (rule.primary_pattern.Matches(primary_url) &&
        rule.secondary_pattern.Matches(secondary_url)) {
      if (primary_pattern)
        *primary_pattern = rule.primary_pattern;
      if (secondary_pattern)
        *secondary_pattern = rule.secondary_pattern;
      return rule.value.get()->DeepCopy();
    }
  }
  return NULL;
}

void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules) {
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES,
      ResourceIdentifier(),
      &(rules->image_rules));
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      ResourceIdentifier(),
      &(rules->script_rules));
}

uint32 PrefRegistrationFlagsForType(ContentSettingsType content_type) {
  uint32 flags = PrefRegistry::NO_REGISTRATION_FLAGS;

  if (IsContentSettingsTypeSyncable(content_type))
    flags |= user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;

  if (IsContentSettingsTypeLossy(content_type))
    flags |= PrefRegistry::LOSSY_PREF;

  return flags;
}

}  // namespace content_settings
