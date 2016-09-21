// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;

namespace {

const char kFlashDownloadURL[] = "get.adobe.com/flash";

bool ShouldInterceptNavigation(
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/626728): Implement permission prompt logic.
  return true;
}

}  // namespace

// static
bool FlashDownloadInterception::ShouldStopFlashDownloadAction(
    HostContentSettingsMap* host_content_settings_map,
    const GURL& source_url,
    const GURL& target_url,
    bool has_user_gesture) {
  if (!base::FeatureList::IsEnabled(features::kPreferHtmlOverPlugins))
    return false;

  if (!has_user_gesture)
    return false;

  if (!base::StartsWith(target_url.GetContent(), kFlashDownloadURL,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  std::unique_ptr<base::Value> general_setting =
      host_content_settings_map->GetWebsiteSetting(
          source_url, source_url, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
          nullptr);
  ContentSetting plugin_setting =
      content_settings::ValueToContentSetting(general_setting.get());
  plugin_setting = PluginsFieldTrial::EffectiveContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, plugin_setting);

  return plugin_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;
}

// static
std::unique_ptr<NavigationThrottle>
FlashDownloadInterception::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Never intercept Flash Download navigations in a new window.
  if (handle->GetWebContents()->HasOpener())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  GURL source_url = handle->GetWebContents()->GetLastCommittedURL();
  if (!ShouldStopFlashDownloadAction(host_content_settings_map, source_url,
                                     handle->GetURL(),
                                     handle->HasUserGesture())) {
    return nullptr;
  }

  return base::MakeUnique<navigation_interception::InterceptNavigationThrottle>(
      handle, base::Bind(&ShouldInterceptNavigation), true);
}
