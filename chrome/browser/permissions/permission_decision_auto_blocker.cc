// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_blacklist_client.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

// The number of times that users may explicitly dismiss a permission prompt
// from an origin before it is automatically blocked.
int g_prompt_dismissals_before_block = 3;

// The number of days that an origin will stay under embargo for a requested
// permission due to blacklisting.
int g_blacklist_embargo_days = 7;

// The number of days that an origin will stay under embargo for a requested
// permission due to repeated dismissals.
int g_dismissal_embargo_days = 7;

std::unique_ptr<base::DictionaryValue> GetOriginDict(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin_url, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
          std::string(), nullptr));
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
const char PermissionDecisionAutoBlocker::kPermissionBlacklistEmbargoKey[] =
    "blacklisting_embargo_days";

// static
const char PermissionDecisionAutoBlocker::kPermissionDismissalEmbargoKey[] =
    "dismissal_embargo_days";

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
bool PermissionDecisionAutoBlocker::RecordDismissAndEmbargo(
    const GURL& url,
    content::PermissionType permission,
    Profile* profile,
    base::Time current_time) {
  int current_dismissal_count = RecordActionInWebsiteSettings(
      url, permission, kPromptDismissCountKey, profile);
  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften) &&
      current_dismissal_count >= g_prompt_dismissals_before_block) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    PlaceUnderEmbargo(permission, url, map, current_time,
                      kPermissionDismissalEmbargoKey);
    return true;
  }
  return false;
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
  int current_dismissal_count =
      RecordDismissAndEmbargo(url, permission, profile, base::Time::Now());

  if (!base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften))
    return false;

  return current_dismissal_count >= g_prompt_dismissals_before_block;
}

// static
void PermissionDecisionAutoBlocker::UpdateFromVariations() {
  int prompt_dismissals = -1;
  int blacklist_embargo_days = -1;
  int dismissal_embargo_days = -1;
  std::string dismissals_value = variations::GetVariationParamValueByFeature(
      features::kBlockPromptsIfDismissedOften, kPromptDismissCountKey);
  std::string blacklist_embargo_value =
      variations::GetVariationParamValueByFeature(
          features::kPermissionsBlacklist, kPermissionBlacklistEmbargoKey);
  std::string dismissal_embargo_value =
      variations::GetVariationParamValueByFeature(
          features::kBlockPromptsIfDismissedOften,
          kPermissionDismissalEmbargoKey);
  // If converting the value fails, stick with the current value.
  if (base::StringToInt(dismissals_value, &prompt_dismissals) &&
      prompt_dismissals > 0) {
    g_prompt_dismissals_before_block = prompt_dismissals;
  }
  if (base::StringToInt(blacklist_embargo_value, &blacklist_embargo_days) &&
      blacklist_embargo_days > 0) {
    g_blacklist_embargo_days = blacklist_embargo_days;
  }
  if (base::StringToInt(dismissal_embargo_value, &dismissal_embargo_days) &&
      dismissal_embargo_days > 0) {
    g_dismissal_embargo_days = dismissal_embargo_days;
  }
}

// static
// TODO(meredithl): Have PermissionDecisionAutoBlocker handle the database
// manager, rather than passing it in.
void PermissionDecisionAutoBlocker::UpdateEmbargoedStatus(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
    content::PermissionType permission,
    const GURL& request_origin,
    content::WebContents* web_contents,
    int timeout,
    Profile* profile,
    base::Time current_time,
    base::Callback<void(bool)> callback) {
  // Check if origin is currently under embargo for the requested permission.
  if (IsUnderEmbargo(permission, profile, request_origin, current_time)) {
    callback.Run(true /* permission_blocked */);
    return;
  }

  if (base::FeatureList::IsEnabled(features::kPermissionsBlacklist) &&
      db_manager) {
    PermissionBlacklistClient::CheckSafeBrowsingBlacklist(
        db_manager, permission, request_origin, web_contents, timeout,
        base::Bind(&PermissionDecisionAutoBlocker::CheckSafeBrowsingResult,
                   permission, profile, request_origin, current_time,
                   callback));
    return;
  }

  callback.Run(false /* permission blocked */);
}

// static
bool PermissionDecisionAutoBlocker::IsUnderEmbargo(
    content::PermissionType permission,
    Profile* profile,
    const GURL& request_origin,
    base::Time current_time) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginDict(map, request_origin);
  base::DictionaryValue* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));
  double embargo_date = -1;
  bool is_under_dismiss_embargo = false;
  bool is_under_blacklist_embargo = false;
  if (base::FeatureList::IsEnabled(features::kPermissionsBlacklist) &&
      permission_dict->GetDouble(kPermissionBlacklistEmbargoKey,
                                 &embargo_date)) {
    if (current_time <
        base::Time::FromInternalValue(embargo_date) +
            base::TimeDelta::FromDays(g_blacklist_embargo_days)) {
      is_under_blacklist_embargo = true;
    }
  }

  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften) &&
      permission_dict->GetDouble(kPermissionDismissalEmbargoKey,
                                 &embargo_date)) {
    if (current_time <
        base::Time::FromInternalValue(embargo_date) +
            base::TimeDelta::FromDays(g_dismissal_embargo_days)) {
      is_under_dismiss_embargo = true;
    }
  }
  // If either embargoes is still in effect, return true.
  return is_under_dismiss_embargo || is_under_blacklist_embargo;
}

void PermissionDecisionAutoBlocker::PlaceUnderEmbargo(
    content::PermissionType permission,
    const GURL& request_origin,
    HostContentSettingsMap* map,
    base::Time current_time,
    const char* key) {
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginDict(map, request_origin);
  base::DictionaryValue* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));
  permission_dict->SetDouble(key, current_time.ToInternalValue());
  map->SetWebsiteSettingDefaultScope(
      request_origin, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
      std::string(), std::move(dict));
}

// static
void PermissionDecisionAutoBlocker::CheckSafeBrowsingResult(
    content::PermissionType permission,
    Profile* profile,
    const GURL& request_origin,
    base::Time current_time,
    base::Callback<void(bool)> callback,
    bool should_be_embargoed) {
  if (should_be_embargoed) {
    // Requesting site is blacklisted for this permission, update the content
    // setting to place it under embargo.
    PlaceUnderEmbargo(permission, request_origin,
                      HostContentSettingsMapFactory::GetForProfile(profile),
                      current_time, kPermissionBlacklistEmbargoKey);
  }
  callback.Run(should_be_embargoed /* permission blocked */);
}
