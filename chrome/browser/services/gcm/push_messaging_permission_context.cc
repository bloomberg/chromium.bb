// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_permission_context.h"

#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

const ContentSettingsType kPushSettingType =
    CONTENT_SETTINGS_TYPE_PUSH_MESSAGING;
const ContentSettingsType kNotificationSettingType =
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS;

namespace gcm {

PushMessagingPermissionContext::PushMessagingPermissionContext(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING),
      profile_(profile) {
}

PushMessagingPermissionContext::~PushMessagingPermissionContext() {
}

ContentSetting PushMessagingPermissionContext::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (requesting_origin != embedding_origin)
    return CONTENT_SETTING_BLOCK;

  ContentSetting push_content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_origin, embedding_origin, kPushSettingType, std::string());

  ContentSetting notifications_content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_origin, embedding_origin, kNotificationSettingType,
          std::string());

  if (notifications_content_setting == CONTENT_SETTING_BLOCK ||
      push_content_setting == CONTENT_SETTING_BLOCK) {
    return CONTENT_SETTING_BLOCK;
  }
  if (notifications_content_setting == CONTENT_SETTING_ASK ||
      push_content_setting == CONTENT_SETTING_ASK) {
    return CONTENT_SETTING_ASK;
  }
  DCHECK_EQ(CONTENT_SETTING_ALLOW, notifications_content_setting);
  DCHECK_EQ(CONTENT_SETTING_ALLOW, push_content_setting);
  return CONTENT_SETTING_ALLOW;
}

// Unlike other permissions, push is decided by the following algorithm
//  - You need to request it from a top level domain
//  - You need to have notification permission granted.
//  - You need to not have push permission explicitly blocked.
//  - If those two things are true it is granted without prompting.
// This is done to avoid double prompting for notifications and push.
void PushMessagingPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  if (requesting_origin != embedding_origin) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, false /* granted */);
  }
  ContentSetting notifications_content_setting =
      profile_->GetHostContentSettingsMap()
          ->GetContentSettingAndMaybeUpdateLastUsage(
              requesting_origin, embedding_origin, kNotificationSettingType,
              std::string());

  if (notifications_content_setting != CONTENT_SETTING_ALLOW) {
    DVLOG(1) << "Notification permission has not been granted.";
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, false /* granted */);
    return;
  }

  ContentSetting push_content_setting =
      profile_->GetHostContentSettingsMap()
          ->GetContentSettingAndMaybeUpdateLastUsage(
              requesting_origin, embedding_origin, kPushSettingType,
              std::string());

  if (push_content_setting == CONTENT_SETTING_BLOCK) {
    DVLOG(1) << "Push permission was explicitly blocked.";
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, false /* granted */);
    return;
  }

  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      true /* persist */, true /* granted */);
}

}  // namespace gcm
