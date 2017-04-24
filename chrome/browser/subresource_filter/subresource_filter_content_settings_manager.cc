// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"

#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

SubresourceFilterContentSettingsManager::
    SubresourceFilterContentSettingsManager(Profile* profile)
    : settings_map_(HostContentSettingsMapFactory::GetForProfile(profile)) {
  DCHECK(profile);
  DCHECK(settings_map_);
  settings_map_->AddObserver(this);
}

SubresourceFilterContentSettingsManager::
    ~SubresourceFilterContentSettingsManager() {
  settings_map_->RemoveObserver(this);
}

void SubresourceFilterContentSettingsManager::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  if (content_type != CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER)
    return;

  const ContentSettingsDetails details(primary_pattern, secondary_pattern,
                                       content_type, resource_identifier);
  if (details.update_all()) {
    ContentSetting global_setting = settings_map_->GetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, nullptr);
    if (global_setting == CONTENT_SETTING_BLOCK) {
      ChromeSubresourceFilterClient::LogAction(
          kActionContentSettingsBlockedGlobal);
    } else if (global_setting == CONTENT_SETTING_ALLOW) {
      ChromeSubresourceFilterClient::LogAction(
          kActionContentSettingsAllowedGlobal);
    } else {
      NOTREACHED();
    }
    return;
  }

  // Remove this DCHECK if extension APIs or admin policies are given the
  // ability to set secondary patterns for this setting.
  DCHECK(secondary_pattern == ContentSettingsPattern::Wildcard());

  DCHECK(primary_pattern.IsValid());

  // An invalid URL indicates that this is a wildcard pattern.
  GURL url = GURL(primary_pattern.ToString());
  if (!url.is_valid()) {
    ChromeSubresourceFilterClient::LogAction(
        kActionContentSettingsWildcardUpdate);
    return;
  }

  ContentSetting setting = settings_map_->GetContentSetting(
      url, url, ContentSettingsType::CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER,
      std::string());
  if (setting == CONTENT_SETTING_BLOCK) {
    ChromeSubresourceFilterClient::LogAction(kActionContentSettingsBlocked);
  } else if (setting == CONTENT_SETTING_ALLOW) {
    ChromeSubresourceFilterClient::LogAction(kActionContentSettingsAllowed);
  } else {
    NOTREACHED();
  }
}
