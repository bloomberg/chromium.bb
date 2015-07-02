// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

NotificationPermissionContext::NotificationPermissionContext(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {}

NotificationPermissionContext::~NotificationPermissionContext() {}

void NotificationPermissionContext::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedder_origin) {
  DesktopNotificationProfileUtil::ClearSetting(
      profile(), ContentSettingsPattern::FromURLNoWildcard(requesting_origin));
}

// Unlike other permission types, granting a notification for a given origin
// will not take into account the |embedder_origin|, it will only be based
// on the requesting iframe origin.
// TODO(mukai) Consider why notifications behave differently than
// other permissions. https://crbug.com/416894
void NotificationPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    ContentSetting content_setting) {
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  if (content_setting == CONTENT_SETTING_ALLOW) {
    DesktopNotificationProfileUtil::GrantPermission(profile(),
                                                    requesting_origin);
  } else {
    DesktopNotificationProfileUtil::DenyPermission(profile(),
                                                   requesting_origin);
  }
}

bool NotificationPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
