// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/content_settings/subresource_filter_infobar_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents), shown_for_navigation_(false) {
  DCHECK(web_contents);
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() {}

void ChromeSubresourceFilterClient::ToggleNotificationVisibility(
    bool visibility) {
  if (shown_for_navigation_ && visibility)
    return;
  shown_for_navigation_ = visibility;
  UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.Prompt.NumVisibility", visibility);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  content_settings->SetSubresourceBlocked(visibility);
#if defined(OS_ANDROID)
  if (visibility) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_);

    SubresourceFilterInfobarDelegate::Create(infobar_service);
    content_settings->SetSubresourceBlockageIndicated();
  }
#endif
}

bool ChromeSubresourceFilterClient::IsWhitelistedByContentSettings(
    const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(profile);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting setting = settings_map->GetContentSetting(
      url, url, ContentSettingsType::CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER,
      std::string());
  return setting == CONTENT_SETTING_BLOCK;
}

void ChromeSubresourceFilterClient::WhitelistByContentSettings(
    const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(profile);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER,
      std::string(), CONTENT_SETTING_BLOCK);
}
