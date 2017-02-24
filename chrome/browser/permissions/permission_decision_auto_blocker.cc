// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_blacklist_client.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/variations/variations_associated_data.h"
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

// Maximum time in milliseconds to wait for safe browsing service to check a
// url for blacklisting. After this amount of time, the check will be aborted
// and the url will be treated as not safe.
// TODO(meredithl): Revisit this once UMA metrics have data about request time.
const int kCheckUrlTimeoutMs = 2000;

std::unique_ptr<base::DictionaryValue> GetOriginDict(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin_url, GURL(), CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
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
                                  ContentSettingsType permission,
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
      url, GURL(), CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
      std::string(), std::move(dict));

  return current_count;
}

int GetActionCount(const GURL& url,
                   ContentSettingsType permission,
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

// PermissionDecisionAutoBlocker::Factory --------------------------------------

// static
PermissionDecisionAutoBlocker*
PermissionDecisionAutoBlocker::Factory::GetForProfile(Profile* profile) {
  return static_cast<PermissionDecisionAutoBlocker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PermissionDecisionAutoBlocker::Factory*
PermissionDecisionAutoBlocker::Factory::GetInstance() {
  return base::Singleton<PermissionDecisionAutoBlocker::Factory>::get();
}

PermissionDecisionAutoBlocker::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "PermissionDecisionAutoBlocker",
          BrowserContextDependencyManager::GetInstance()) {}

PermissionDecisionAutoBlocker::Factory::~Factory() {}

KeyedService* PermissionDecisionAutoBlocker::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new PermissionDecisionAutoBlocker(profile);
}

content::BrowserContext*
PermissionDecisionAutoBlocker::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

// PermissionDecisionAutoBlocker -----------------------------------------------

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
PermissionDecisionAutoBlocker* PermissionDecisionAutoBlocker::GetForProfile(
    Profile* profile) {
  return PermissionDecisionAutoBlocker::Factory::GetForProfile(profile);
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

void PermissionDecisionAutoBlocker::CheckSafeBrowsingBlacklist(
    content::WebContents* web_contents,
    const GURL& request_origin,
    ContentSettingsType permission,
    base::Callback<void(bool)> callback) {
  DCHECK_EQ(CONTENT_SETTING_ASK,
            GetEmbargoResult(request_origin, permission).content_setting);

  if (base::FeatureList::IsEnabled(features::kPermissionsBlacklist) &&
      db_manager_) {
    // The CheckSafeBrowsingResult callback won't be called if the profile is
    // destroyed before a result is received. In that case this object will have
    // been destroyed by that point.
    PermissionBlacklistClient::CheckSafeBrowsingBlacklist(
        web_contents, db_manager_, request_origin, permission,
        safe_browsing_timeout_,
        base::Bind(&PermissionDecisionAutoBlocker::CheckSafeBrowsingResult,
                   base::Unretained(this), request_origin, permission,
                   callback));
    return;
  }

  callback.Run(false /* permission blocked */);
}

PermissionResult PermissionDecisionAutoBlocker::GetEmbargoResult(
    const GURL& request_origin,
    ContentSettingsType permission) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginDict(map, request_origin);
  base::DictionaryValue* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));
  double embargo_date = -1;

  base::Time current_time = clock_->Now();
  if (base::FeatureList::IsEnabled(features::kPermissionsBlacklist) &&
      permission_dict->GetDouble(kPermissionBlacklistEmbargoKey,
                                 &embargo_date)) {
    if (current_time <
        base::Time::FromInternalValue(embargo_date) +
            base::TimeDelta::FromDays(g_blacklist_embargo_days)) {
      return PermissionResult(CONTENT_SETTING_BLOCK,
                              PermissionStatusSource::SAFE_BROWSING_BLACKLIST);
    }
  }

  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften) &&
      permission_dict->GetDouble(kPermissionDismissalEmbargoKey,
                                 &embargo_date)) {
    if (current_time <
        base::Time::FromInternalValue(embargo_date) +
            base::TimeDelta::FromDays(g_dismissal_embargo_days)) {
      return PermissionResult(CONTENT_SETTING_BLOCK,
                              PermissionStatusSource::MULTIPLE_DISMISSALS);
    }
  }

  return PermissionResult(CONTENT_SETTING_ASK,
                          PermissionStatusSource::UNSPECIFIED);
}

int PermissionDecisionAutoBlocker::GetDismissCount(
    const GURL& url,
    ContentSettingsType permission) {
  return GetActionCount(url, permission, kPromptDismissCountKey, profile_);
}

int PermissionDecisionAutoBlocker::GetIgnoreCount(
    const GURL& url,
    ContentSettingsType permission) {
  return GetActionCount(url, permission, kPromptIgnoreCountKey, profile_);
}

bool PermissionDecisionAutoBlocker::RecordDismissAndEmbargo(
    const GURL& url,
    ContentSettingsType permission) {
  int current_dismissal_count = RecordActionInWebsiteSettings(
      url, permission, kPromptDismissCountKey, profile_);

  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften) &&
      current_dismissal_count >= g_prompt_dismissals_before_block) {
    PlaceUnderEmbargo(url, permission, kPermissionDismissalEmbargoKey);
    return true;
  }
  return false;
}

int PermissionDecisionAutoBlocker::RecordIgnore(
    const GURL& url,
    ContentSettingsType permission) {
  return RecordActionInWebsiteSettings(url, permission, kPromptIgnoreCountKey,
                                       profile_);
}

void PermissionDecisionAutoBlocker::RemoveCountsByUrl(
    base::Callback<bool(const GURL& url)> filter) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  std::unique_ptr<ContentSettingsForOneType> settings(
      new ContentSettingsForOneType);
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
                             std::string(), settings.get());

  for (const auto& site : *settings) {
    GURL origin(site.primary_pattern.ToString());

    if (origin.is_valid() && filter.Run(origin)) {
      map->SetWebsiteSettingDefaultScope(
          origin, GURL(), CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
          std::string(), nullptr);
    }
  }
}

PermissionDecisionAutoBlocker::PermissionDecisionAutoBlocker(Profile* profile)
    : profile_(profile),
      db_manager_(nullptr),
      safe_browsing_timeout_(kCheckUrlTimeoutMs),
      clock_(new base::DefaultClock()) {
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service)
    db_manager_ = sb_service->database_manager();
}

PermissionDecisionAutoBlocker::~PermissionDecisionAutoBlocker() {}

void PermissionDecisionAutoBlocker::CheckSafeBrowsingResult(
    const GURL& request_origin,
    ContentSettingsType permission,
    base::Callback<void(bool)> callback,
    bool should_be_embargoed) {
  if (should_be_embargoed) {
    // Requesting site is blacklisted for this permission, update the content
    // setting to place it under embargo.
    PlaceUnderEmbargo(request_origin, permission,
                      kPermissionBlacklistEmbargoKey);
  }
  callback.Run(should_be_embargoed /* permission blocked */);
}

void PermissionDecisionAutoBlocker::PlaceUnderEmbargo(
    const GURL& request_origin,
    ContentSettingsType permission,
    const char* key) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginDict(map, request_origin);
  base::DictionaryValue* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));
  permission_dict->SetDouble(key, clock_->Now().ToInternalValue());
  map->SetWebsiteSettingDefaultScope(
      request_origin, GURL(), CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
      std::string(), std::move(dict));
}

void PermissionDecisionAutoBlocker::
    SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
        int timeout) {
  db_manager_ = db_manager;
  safe_browsing_timeout_ = timeout;
}

void PermissionDecisionAutoBlocker::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}
