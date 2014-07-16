// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "url/gurl.h"

namespace {

// The names of the ContentSettingsType values, for use with dictionary prefs.
const char* kTypeNames[] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  "geolocation",
  "notifications",
  "auto-select-certificate",
  "fullscreen",
  "mouselock",
  "mixed-script",
  "media-stream",
  "media-stream-mic",
  "media-stream-camera",
  "register-protocol-handler",
  "ppapi-broker",
  "multiple-automatic-downloads",
  "midi-sysex",
  "push-messaging",
#if defined(OS_WIN)
  "metro-switch-to-desktop",
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
  "protected-media-identifier",
#endif
#if defined(OS_ANDROID)
  "app-banner",
#endif
};
COMPILE_ASSERT(arraysize(kTypeNames) == CONTENT_SETTINGS_NUM_TYPES,
               type_names_incorrect_size);

const char kPatternSeparator[] = ",";

}  // namespace

namespace content_settings {

std::string GetTypeName(ContentSettingsType type) {
  return std::string(kTypeNames[type]);
}

bool GetTypeFromName(const std::string& name,
                     ContentSettingsType* return_setting) {
  for (size_t type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    if (name.compare(kTypeNames[type]) == 0) {
      *return_setting = static_cast<ContentSettingsType>(type);
      return true;
    }
  }
  return false;
}

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern) {
  return item_pattern.ToString()
         + std::string(kPatternSeparator)
         + top_level_frame_pattern.ToString();
}

PatternPair ParsePatternString(const std::string& pattern_str) {
  std::vector<std::string> pattern_str_list;
  base::SplitString(pattern_str, kPatternSeparator[0], &pattern_str_list);

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

base::Value* GetContentSettingValue(const ProviderInterface* provider,
                                    const GURL& primary_url,
                                    const GURL& secondary_url,
                                    ContentSettingsType content_type,
                                    const std::string& resource_identifier,
                                    bool include_incognito) {
  return GetContentSettingValueAndPatterns(provider, primary_url, secondary_url,
                               content_type, resource_identifier,
                               include_incognito, NULL, NULL);
}

ContentSetting GetContentSetting(const ProviderInterface* provider,
                                 const GURL& primary_url,
                                 const GURL& secondary_url,
                                 ContentSettingsType content_type,
                                 const std::string& resource_identifier,
                                 bool include_incognito) {
  scoped_ptr<base::Value> value(
      GetContentSettingValue(provider, primary_url, secondary_url,
                             content_type, resource_identifier,
                             include_incognito));
  return ValueToContentSetting(value.get());
}

void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules) {
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES, std::string(), &(rules->image_rules));
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(), &(rules->script_rules));
}

}  // namespace content_settings
