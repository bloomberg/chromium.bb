// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include "base/bind.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"
#include "ui/base/text/bytes_formatting.h"

namespace settings {

SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile), observer_(this) {
  observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile));
  if (profile->HasOffTheRecordProfile()) {
    auto map = HostContentSettingsMapFactory::GetForProfile(
        profile->GetOffTheRecordProfile());
    if (!observer_.IsObserving(map))
      observer_.Add(map);
  }
}

SiteSettingsHandler::~SiteSettingsHandler() {
}

void SiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchUsageTotal",
      base::Bind(&SiteSettingsHandler::HandleFetchUsageTotal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearUsage",
      base::Bind(&SiteSettingsHandler::HandleClearUsage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefaultValueForContentType",
      base::Bind(&SiteSettingsHandler::HandleSetDefaultValueForContentType,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDefaultValueForContentType",
      base::Bind(&SiteSettingsHandler::HandleGetDefaultValueForContentType,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExceptionList",
      base::Bind(&SiteSettingsHandler::HandleGetExceptionList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetCategoryPermissionForOrigin",
      base::Bind(&SiteSettingsHandler::HandleResetCategoryPermissionForOrigin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setCategoryPermissionForOrigin",
      base::Bind(&SiteSettingsHandler::HandleSetCategoryPermissionForOrigin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isPatternValid",
      base::Bind(&SiteSettingsHandler::HandleIsPatternValid,
                 base::Unretained(this)));
}

void SiteSettingsHandler::OnGetUsageInfo(
    const storage::UsageInfoEntries& entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& entry : entries) {
    if (entry.usage <= 0) continue;
    if (entry.host == usage_host_) {
      web_ui()->CallJavascriptFunction(
         "settings.WebsiteUsagePrivateApi.returnUsageTotal",
         base::StringValue(entry.host),
         base::StringValue(ui::FormatBytes(entry.usage)),
         base::FundamentalValue(entry.type));
      return;
    }
  }
}

void SiteSettingsHandler::OnUsageInfoCleared(storage::QuotaStatusCode code) {
  if (code == storage::kQuotaStatusOk) {
    web_ui()->CallJavascriptFunction(
        "settings.WebsiteUsagePrivateApi.onUsageCleared",
        base::StringValue(clearing_origin_));
  }
}

void SiteSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  if (primary_pattern.ToString().empty()) {
    web_ui()->CallJavascriptFunction(
        "cr.webUIListenerCallback",
        base::StringValue("contentSettingCategoryChanged"),
        base::FundamentalValue(content_type));
  } else {
    web_ui()->CallJavascriptFunction(
        "cr.webUIListenerCallback",
        base::StringValue("contentSettingSitePermissionChanged"),
        base::FundamentalValue(content_type),
        base::StringValue(primary_pattern.ToString()));
  }
}

void SiteSettingsHandler::HandleFetchUsageTotal(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string host;
  CHECK(args->GetString(0, &host));
  usage_host_ = host;

  scoped_refptr<StorageInfoFetcher> storage_info_fetcher
      = new StorageInfoFetcher(profile_);
  storage_info_fetcher->FetchStorageInfo(
      base::Bind(&SiteSettingsHandler::OnGetUsageInfo, base::Unretained(this)));
}

void SiteSettingsHandler::HandleClearUsage(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string origin;
  CHECK(args->GetString(0, &origin));
  double type;
  CHECK(args->GetDouble(1, &type));

  GURL url(origin);
  if (url.is_valid()) {
    clearing_origin_ = origin;

    // Start by clearing the storage data asynchronously.
    scoped_refptr<StorageInfoFetcher> storage_info_fetcher
        = new StorageInfoFetcher(profile_);
    storage_info_fetcher->ClearStorage(
        url.host(),
        static_cast<storage::StorageType>(static_cast<int>(type)),
        base::Bind(&SiteSettingsHandler::OnUsageInfoCleared,
            base::Unretained(this)));

    // Also clear the *local* storage data.
    scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper =
        new BrowsingDataLocalStorageHelper(profile_);
    local_storage_helper->DeleteOrigin(url);
  }
}

void SiteSettingsHandler::HandleSetDefaultValueForContentType(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  double content_type;
  CHECK(args->GetDouble(0, &content_type));
  std::string setting;
  CHECK(args->GetString(1, &setting));
  ContentSetting default_setting;
  CHECK(content_settings::ContentSettingFromString(setting, &default_setting));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  map->SetDefaultContentSetting(
      static_cast<ContentSettingsType>(static_cast<int>(content_type)),
      default_setting);
}

void SiteSettingsHandler::HandleGetDefaultValueForContentType(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  double type;
  CHECK(args->GetDouble(1, &type));

  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(type));
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  ContentSetting setting = map->GetDefaultContentSetting(content_type, nullptr);

  // FullScreen is Allow vs. Ask.
  bool enabled;
  if (content_type == CONTENT_SETTINGS_TYPE_FULLSCREEN)
      enabled = setting != CONTENT_SETTING_ASK;
  else
      enabled = setting != CONTENT_SETTING_BLOCK;

  ResolveJavascriptCallback(*callback_id, base::FundamentalValue(enabled));
}

void SiteSettingsHandler::HandleGetExceptionList(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  double type;
  CHECK(args->GetDouble(1, &type));
  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(type));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::ListValue> exceptions(new base::ListValue);
  site_settings::GetExceptionsFromHostContentSettingsMap(
      map, content_type, web_ui(), exceptions.get());
  ResolveJavascriptCallback(*callback_id, *exceptions.get());
}

void SiteSettingsHandler::HandleResetCategoryPermissionForOrigin(
    const base::ListValue* args) {
  CHECK_EQ(3U, args->GetSize());
  std::string primary_pattern;
  CHECK(args->GetString(0, &primary_pattern));
  std::string secondary_pattern;
  CHECK(args->GetString(1, &secondary_pattern));
  double type;
  CHECK(args->GetDouble(2, &type));

  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(type));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(primary_pattern),
      secondary_pattern.empty() ?
          ContentSettingsPattern::Wildcard() :
          ContentSettingsPattern::FromString(secondary_pattern),
      content_type, "", CONTENT_SETTING_DEFAULT);
}

void SiteSettingsHandler::HandleSetCategoryPermissionForOrigin(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());
  std::string primary_pattern;
  CHECK(args->GetString(0, &primary_pattern));
  std::string secondary_pattern;
  CHECK(args->GetString(1, &secondary_pattern));
  double type;
  CHECK(args->GetDouble(2, &type));
  std::string value;
  CHECK(args->GetString(3, &value));

  ContentSettingsType content_type =
      static_cast<ContentSettingsType>(static_cast<int>(type));
  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(primary_pattern),
      secondary_pattern.empty() ?
          ContentSettingsPattern::Wildcard() :
          ContentSettingsPattern::FromString(secondary_pattern),
      content_type, "", setting);
}

void SiteSettingsHandler::HandleIsPatternValid(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string pattern_string;
  CHECK(args->GetString(1, &pattern_string));

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_string);
  ResolveJavascriptCallback(
      *callback_id, base::FundamentalValue(pattern.IsValid()));
}

}  // namespace settings
