// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_permission_context.h"

#include "chrome/browser/content_settings/permission_context_uma_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

const ContentSettingsType kPushSettingType =
    CONTENT_SETTINGS_TYPE_PUSH_MESSAGING;

PushMessagingPermissionContext::PushMessagingPermissionContext(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING),
      profile_(profile),
      weak_factory_ui_thread_(this) {
}

PushMessagingPermissionContext::~PushMessagingPermissionContext() {
}

ContentSetting PushMessagingPermissionContext::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
#if defined(ENABLE_NOTIFICATIONS)
  if (requesting_origin != embedding_origin)
    return CONTENT_SETTING_BLOCK;

  ContentSetting push_content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_origin, embedding_origin, kPushSettingType, std::string());

  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile_);
  DCHECK(notification_service);

  ContentSetting notifications_permission =
      notification_service->GetPermissionStatus(requesting_origin,
                                                embedding_origin);

  if (notifications_permission == CONTENT_SETTING_BLOCK ||
      push_content_setting == CONTENT_SETTING_BLOCK) {
    return CONTENT_SETTING_BLOCK;
  }
  if (notifications_permission == CONTENT_SETTING_ASK ||
      push_content_setting == CONTENT_SETTING_ASK) {
    return CONTENT_SETTING_ASK;
  }
  DCHECK_EQ(CONTENT_SETTING_ALLOW, notifications_permission);
  DCHECK_EQ(CONTENT_SETTING_ALLOW, push_content_setting);
  return CONTENT_SETTING_ALLOW;
#else
  return CONTENT_SETTING_BLOCK;
#endif
}

void PushMessagingPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents, const PermissionRequestID& id) {
  // TODO(peter): consider implementing this method.
  NOTIMPLEMENTED() << "CancelPermission not implemented for push messaging";
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(ENABLE_NOTIFICATIONS)
  if (requesting_origin != embedding_origin) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile_);
  DCHECK(notification_service);

  notification_service->RequestPermission(
      web_contents, id, requesting_origin, user_gesture,
      base::Bind(&PushMessagingPermissionContext::DecidePushPermission,
                 weak_factory_ui_thread_.GetWeakPtr(), id, requesting_origin,
                 embedding_origin, callback));
#else
  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      false /* persist */, CONTENT_SETTING_BLOCK);
#endif
}

void PushMessagingPermissionContext::DecidePushPermission(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    ContentSetting notification_content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ContentSetting push_content_setting =
      profile_->GetHostContentSettingsMap()
          ->GetContentSettingAndMaybeUpdateLastUsage(
              requesting_origin, embedding_origin, kPushSettingType,
              std::string());

  if (push_content_setting == CONTENT_SETTING_BLOCK) {
    DVLOG(1) << "Push permission was explicitly blocked.";
    PermissionContextUmaUtil::PermissionDenied(kPushSettingType,
                                               requesting_origin);
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        true /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  if (notification_content_setting != CONTENT_SETTING_ALLOW) {
    DVLOG(1) << "Notification permission has not been granted.";
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, notification_content_setting);
    return;
  }

  PermissionContextUmaUtil::PermissionGranted(kPushSettingType,
                                              requesting_origin);
  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      true /* persist */, CONTENT_SETTING_ALLOW);
}
