// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager_factory.h"
#include "chrome/browser/ui/android/content_settings/subresource_filter_infobar_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/content/browser/content_ruleset_service.h"
#include "content/public/browser/navigation_handle.h"

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents), shown_for_navigation_(false) {
  DCHECK(web_contents);
  // Ensure the content settings manager is initialized.
  SubresourceFilterContentSettingsManagerFactory::EnsureForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() {}

void ChromeSubresourceFilterClient::ToggleNotificationVisibility(
    bool visibility) {
  if (shown_for_navigation_ && visibility)
    return;

  shown_for_navigation_ = visibility;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);

  // |visibility| is false when a new navigation starts.
  if (visibility) {
    content_settings->OnContentBlocked(
        CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER);
    LogAction(kActionUIShown);
#if defined(OS_ANDROID)
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_);
    SubresourceFilterInfobarDelegate::Create(infobar_service);
#endif
  } else {
    LogAction(kActionNavigationStarted);
  }
}

bool ChromeSubresourceFilterClient::ShouldSuppressActivation(
    content::NavigationHandle* navigation_handle) {
  const GURL& url(navigation_handle->GetURL());
  return navigation_handle->IsInMainFrame() &&
         (whitelisted_hosts_.find(url.host()) != whitelisted_hosts_.end() ||
          GetContentSettingForUrl(url) == CONTENT_SETTING_BLOCK);
}

void ChromeSubresourceFilterClient::WhitelistByContentSettings(
    const GURL& url) {
  // Whitelist via content settings.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(profile);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER,
      std::string(), CONTENT_SETTING_BLOCK);

  LogAction(kActionContentSettingsBlockedFromUI);
}

void ChromeSubresourceFilterClient::WhitelistInCurrentWebContents(
    const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS())
    whitelisted_hosts_.insert(url.host());
}

// static
void ChromeSubresourceFilterClient::LogAction(SubresourceFilterAction action) {
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.Actions", action,
                            kActionLastEntry);
}

ContentSetting ChromeSubresourceFilterClient::GetContentSettingForUrl(
    const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(profile);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return settings_map->GetContentSetting(
      url, url, ContentSettingsType::CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER,
      std::string());
}

subresource_filter::VerifiedRulesetDealer::Handle*
ChromeSubresourceFilterClient::GetRulesetDealer() {
  subresource_filter::ContentRulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  return ruleset_service ? ruleset_service->ruleset_dealer() : nullptr;
}
