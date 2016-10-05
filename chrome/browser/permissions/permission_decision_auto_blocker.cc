// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"

namespace {

// The number of times that users may explicitly dismiss a permission prompt
// from an origin before it is automatically blocked.
int g_prompt_dismissals_before_block = 3;

std::unique_ptr<base::DictionaryValue> GetOriginDict(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin_url, origin_url,
          CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT, std::string(),
          nullptr));
  if (!dict)
    return base::MakeUnique<base::DictionaryValue>();

  return dict;
}

base::DictionaryValue* GetOrCreatePermissionDict(
    base::DictionaryValue* origin_dict,
    const std::string& permission) {
  base::DictionaryValue* permission_dict = nullptr;
  if (!origin_dict->GetDictionaryWithoutPathExpansion(permission,
                                                      &permission_dict)) {
    permission_dict = new base::DictionaryValue();
    origin_dict->SetWithoutPathExpansion(permission,
                                         base::WrapUnique(permission_dict));
  }

  return permission_dict;
}

int RecordActionInWebsiteSettings(const GURL& url,
                                  content::PermissionType permission,
                                  const char* key,
                                  Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> dict = GetOriginDict(map, url);

  base::DictionaryValue* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));

  int current_count = 0;
  permission_dict->GetInteger(key, &current_count);
  permission_dict->SetInteger(key, ++current_count);

  map->SetWebsiteSettingDefaultScope(
      url, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
      std::string(), std::move(dict));

  return current_count;
}

int GetActionCount(const GURL& url,
                   content::PermissionType permission,
                   const char* key,
                   Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> dict = GetOriginDict(map, url);

  base::DictionaryValue* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));

  int current_count = 0;
  permission_dict->GetInteger(key, &current_count);
  return current_count;
}

}  // namespace

// static
const char PermissionDecisionAutoBlocker::kPromptDismissCountKey[] =
    "dismiss_count";

// static
const char PermissionDecisionAutoBlocker::kPromptIgnoreCountKey[] =
    "ignore_count";

// static
void PermissionDecisionAutoBlocker::RemoveCountsByUrl(
    Profile* profile,
    base::Callback<bool(const GURL& url)> filter) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  std::unique_ptr<ContentSettingsForOneType> settings(
      new ContentSettingsForOneType);
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
                             std::string(), settings.get());

  for (const auto& site : *settings) {
    GURL origin(site.primary_pattern.ToString());

    if (origin.is_valid() && filter.Run(origin)) {
      map->SetWebsiteSettingDefaultScope(
          origin, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
          std::string(), nullptr);
    }
  }
}

// static
int PermissionDecisionAutoBlocker::GetDismissCount(
    const GURL& url,
    content::PermissionType permission,
    Profile* profile) {
  return GetActionCount(url, permission, kPromptDismissCountKey, profile);
}

// static
int PermissionDecisionAutoBlocker::GetIgnoreCount(
    const GURL& url,
    content::PermissionType permission,
    Profile* profile) {
  return GetActionCount(url, permission, kPromptIgnoreCountKey, profile);
}

// static
int PermissionDecisionAutoBlocker::RecordDismiss(
    const GURL& url,
    content::PermissionType permission,
    Profile* profile) {
  return RecordActionInWebsiteSettings(url, permission, kPromptDismissCountKey,
                                       profile);
}

// static
int PermissionDecisionAutoBlocker::RecordIgnore(
    const GURL& url,
    content::PermissionType permission,
    Profile* profile) {
  return RecordActionInWebsiteSettings(url, permission, kPromptIgnoreCountKey,
                                       profile);
}

// static
bool PermissionDecisionAutoBlocker::ShouldChangeDismissalToBlock(
    const GURL& url,
    content::PermissionType permission,
    Profile* profile) {
  int current_dismissal_count = RecordDismiss(url, permission, profile);

  if (!base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften))
    return false;

  return current_dismissal_count >= g_prompt_dismissals_before_block;
}

// static
void PermissionDecisionAutoBlocker::UpdateFromVariations() {
  int prompt_dismissals = -1;
  std::string value = variations::GetVariationParamValueByFeature(
      features::kBlockPromptsIfDismissedOften, kPromptDismissCountKey);

  // If converting the value fails, stick with the current value.
  if (base::StringToInt(value, &prompt_dismissals) && prompt_dismissals > 0)
    g_prompt_dismissals_before_block = prompt_dismissals;
}
