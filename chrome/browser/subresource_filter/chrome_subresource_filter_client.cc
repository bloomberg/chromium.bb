// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/android/content_settings/subresource_filter_infobar_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/v4_feature_list.h"
#include "components/subresource_filter/content/browser/content_ruleset_service.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeSubresourceFilterClient);

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents), did_show_ui_for_navigation_(false) {
  DCHECK(web_contents);
  SubresourceFilterProfileContextFactory::EnsureForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));

  subresource_filter::ContentSubresourceFilterDriverFactory::
      CreateForWebContents(web_contents, this);
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() {}

void ChromeSubresourceFilterClient::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  // Don't add any throttles if the feature isn't enabled at all.
  if (subresource_filter::GetActiveConfigurations()
          ->the_one_and_only()
          .activation_scope == subresource_filter::ActivationScope::NO_SITES) {
    return;
  }

  if (navigation_handle->IsInMainFrame()) {
    safe_browsing::SafeBrowsingService* safe_browsing_service =
        g_browser_process->safe_browsing_service();
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager;
    if (safe_browsing_service &&
        safe_browsing_service->database_manager()->IsSupported() &&
        safe_browsing::V4FeatureList::GetV4UsageStatus() ==
            safe_browsing::V4FeatureList::V4UsageStatus::V4_ONLY) {
      database_manager = safe_browsing_service->database_manager();
    }

    throttles->push_back(
        base::MakeUnique<subresource_filter::
                             SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle,
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::IO),
            std::move(database_manager)));
  }

  auto* driver_factory =
      subresource_filter::ContentSubresourceFilterDriverFactory::
          FromWebContents(navigation_handle->GetWebContents());
  driver_factory->throttle_manager()->MaybeAppendNavigationThrottles(
      navigation_handle, throttles);
}

void ChromeSubresourceFilterClient::ToggleNotificationVisibility(
    bool visibility) {
  if (did_show_ui_for_navigation_ && visibility)
    return;

  did_show_ui_for_navigation_ = visibility;
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
