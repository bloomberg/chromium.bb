// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"

using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;

namespace {

// Regexes matching
const char kGetFlashURLCanonicalRegex[] = "(?i)get\\.adobe\\.com/.*flash.*";
const char kGetFlashURLSecondaryRegex[] =
    "(?i)(www\\.)?(adobe|macromedia)\\.com/go.*"
    "(get[-_]?flash|fl(ash)?.?pl(ayer)?|flash_completion|flashpm|flashdownload|"
    "fp|h-m-a-?2|chrome|download_player|gnav_fl|pdcredirect).*";

void DoNothing(blink::mojom::PermissionStatus result) {}

bool InterceptNavigation(
    const GURL& source_url,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  FlashDownloadInterception::InterceptFlashDownloadNavigation(source,
                                                              source_url);
  return true;
}

}  // namespace

// static
void FlashDownloadInterception::InterceptFlashDownloadNavigation(
    content::WebContents* web_contents,
    const GURL& source_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
      host_content_settings_map, url::Origin(source_url), source_url, nullptr);
  flash_setting = PluginsFieldTrial::EffectiveContentSetting(
      host_content_settings_map, CONTENT_SETTINGS_TYPE_PLUGINS, flash_setting);

  if (flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT) {
    PermissionManager* manager = PermissionManager::Get(profile);
    manager->RequestPermission(
        content::PermissionType::FLASH, web_contents->GetMainFrame(),
        web_contents->GetLastCommittedURL(), true, base::Bind(&DoNothing));
  } else if (flash_setting == CONTENT_SETTING_BLOCK) {
    TabSpecificContentSettings::FromWebContents(web_contents)
        ->FlashDownloadBlocked();
  }

  // If the content setting has been already changed, do nothing.
}

// static
bool FlashDownloadInterception::ShouldStopFlashDownloadAction(
    HostContentSettingsMap* host_content_settings_map,
    const GURL& source_url,
    const GURL& target_url,
    bool has_user_gesture) {
  if (!PluginUtils::ShouldPreferHtmlOverPlugins(host_content_settings_map))
    return false;

  if (!has_user_gesture)
    return false;

  // If the navigation source is already the Flash download page, don't
  // intercept the download. The user may be trying to download Flash.
  if (RE2::PartialMatch(source_url.GetContent(), kGetFlashURLCanonicalRegex))
    return false;

  std::string target_url_str = target_url.GetContent();
  if (RE2::FullMatch(target_url_str, kGetFlashURLCanonicalRegex) ||
      RE2::FullMatch(target_url_str, kGetFlashURLSecondaryRegex)) {
    ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
        host_content_settings_map, url::Origin(source_url), source_url,
        nullptr);
    flash_setting = PluginsFieldTrial::EffectiveContentSetting(
        host_content_settings_map, CONTENT_SETTINGS_TYPE_PLUGINS,
        flash_setting);

    return flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT ||
           flash_setting == CONTENT_SETTING_BLOCK;
  }

  return false;
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
      handle, base::Bind(&InterceptNavigation, source_url), true);
}
