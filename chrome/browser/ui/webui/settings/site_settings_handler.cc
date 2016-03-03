// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"
#include "ui/base/text/bytes_formatting.h"

namespace settings {

SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile) {
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
  int type;
  CHECK(args->GetInteger(1, &type));

  GURL url(origin);
  if (url.is_valid()) {
    clearing_origin_ = origin;

    // Start by clearing the storage data asynchronously.
    scoped_refptr<StorageInfoFetcher> storage_info_fetcher
        = new StorageInfoFetcher(profile_);
    storage_info_fetcher->ClearStorage(
        url.host(),
        static_cast<storage::StorageType>(type),
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
  int content_type;
  CHECK(args->GetInteger(0, &content_type));
  int default_setting;
  CHECK(args->GetInteger(1, &default_setting));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  map->SetDefaultContentSetting(static_cast<ContentSettingsType>(content_type),
                                static_cast<ContentSetting>(default_setting));
}

void SiteSettingsHandler::HandleGetDefaultValueForContentType(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  int type;
  CHECK(args->GetInteger(1, &type));

  ContentSettingsType content_type = static_cast<ContentSettingsType>(type);
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

}  // namespace settings
