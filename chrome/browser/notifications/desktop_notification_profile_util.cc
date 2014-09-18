// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_profile_util.h"

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

void DesktopNotificationProfileUtil::ResetToDefaultContentSetting(
    Profile* profile) {
  profile->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_DEFAULT);
}

// Clears the notifications setting for the given pattern.
void DesktopNotificationProfileUtil::ClearSetting(
    Profile* profile, const ContentSettingsPattern& pattern) {
  profile->GetHostContentSettingsMap()->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER,
      CONTENT_SETTING_DEFAULT);

}

// Methods to setup and modify permission preferences.
void DesktopNotificationProfileUtil::GrantPermission(
    Profile* profile, const GURL& origin) {
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin);
    profile->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        NO_RESOURCE_IDENTIFIER,
        CONTENT_SETTING_ALLOW);
}

void DesktopNotificationProfileUtil::DenyPermission(
    Profile* profile, const GURL& origin) {
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin);
    profile->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        NO_RESOURCE_IDENTIFIER,
        CONTENT_SETTING_BLOCK);
}

void DesktopNotificationProfileUtil::GetNotificationsSettings(
    Profile* profile, ContentSettingsForOneType* settings) {
  profile->GetHostContentSettingsMap()->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER,
      settings);
}

ContentSetting DesktopNotificationProfileUtil::GetContentSetting(
    Profile* profile, const GURL& origin) {
  return profile->GetHostContentSettingsMap()->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER);
}

void DesktopNotificationProfileUtil::UsePermission(Profile* profile,
                                                   const GURL& origin) {
  profile->GetHostContentSettingsMap()->UpdateLastUsageByPattern(
      ContentSettingsPattern::FromURLNoWildcard(origin),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}
