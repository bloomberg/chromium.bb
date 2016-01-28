// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/text/bytes_formatting.h"

namespace settings {

SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile) {
}

SiteSettingsHandler::~SiteSettingsHandler() {
  if (storage_info_fetcher_.get())
    storage_info_fetcher_->RemoveObserver(this);
}

void SiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchUsageTotal",
      base::Bind(&SiteSettingsHandler::HandleFetchUsageTotal,
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
         base::StringValue(ui::FormatBytes(entry.usage)));
      return;
    }
  }
}

void SiteSettingsHandler::HandleFetchUsageTotal(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string host;
  CHECK(args->GetString(0, &host));
  usage_host_ = host;
  storage_info_fetcher_ = new StorageInfoFetcher(
      content::BrowserContext::GetDefaultStoragePartition(
          profile_)->GetQuotaManager());
  storage_info_fetcher_->AddObserver(this);
  storage_info_fetcher_->Run();
}

}  // namespace settings
