// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/origin_util.h"

PushMessagingPermissionContext::PushMessagingPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            content::PermissionType::PUSH_MESSAGING,
                            CONTENT_SETTINGS_TYPE_PUSH_MESSAGING),
      profile_(profile),
      weak_factory_ui_thread_(this) {}

PushMessagingPermissionContext::~PushMessagingPermissionContext() {}

ContentSetting PushMessagingPermissionContext::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // It's possible for this to return CONTENT_SETTING_BLOCK in cases where
  // HostContentSettingsMap::GetContentSetting returns CONTENT_SETTING_ALLOW.
  // TODO(johnme): This is likely to break assumptions made elsewhere, so we
  // should try to remove this quirk.
#if defined(ENABLE_NOTIFICATIONS)
  if (requesting_origin != embedding_origin)
    return CONTENT_SETTING_BLOCK;

  ContentSetting push_content_setting =
      PermissionContextBase::GetPermissionStatus(requesting_origin,
                                                 embedding_origin);

  blink::mojom::PermissionStatus notifications_permission =
      PermissionManager::Get(profile_)->GetPermissionStatus(
          content::PermissionType::NOTIFICATIONS, requesting_origin,
          embedding_origin);

  if (notifications_permission == blink::mojom::PermissionStatus::DENIED ||
      push_content_setting == CONTENT_SETTING_BLOCK) {
    return CONTENT_SETTING_BLOCK;
  }
  if (notifications_permission == blink::mojom::PermissionStatus::ASK)
    return CONTENT_SETTING_ASK;

  DCHECK(push_content_setting == CONTENT_SETTING_ALLOW ||
         push_content_setting == CONTENT_SETTING_ASK);

  // If the notifications permission has already been granted,
  // and the push permission isn't explicitly blocked, then grant
  // allow permission.
  return CONTENT_SETTING_ALLOW;
#else
  return CONTENT_SETTING_BLOCK;
#endif
}

// Unlike other permissions, push is decided by the following algorithm
//  - You need to request it from a top level domain
//  - You need to have notification permission granted.
//  - You need to not have push permission explicitly blocked.
//  - If those 3 things are true it is granted without prompting.
// This is done to avoid double prompting for notifications and push.
void PushMessagingPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(ENABLE_NOTIFICATIONS)
  if (requesting_origin != embedding_origin) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  PermissionManager::Get(profile_)->RequestPermission(
      content::PermissionType::NOTIFICATIONS, web_contents->GetMainFrame(),
      requesting_origin,
      base::Bind(&PushMessagingPermissionContext::DecidePushPermission,
                 weak_factory_ui_thread_.GetWeakPtr(), id, requesting_origin,
                 embedding_origin, callback));
#else
  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      false /* persist */, CONTENT_SETTING_BLOCK);
#endif
}

bool PushMessagingPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

void PushMessagingPermissionContext::DecidePushPermission(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    blink::mojom::PermissionStatus notification_status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(notification_status, blink::mojom::PermissionStatus::ASK);

  ContentSetting push_content_setting =
      HostContentSettingsMapFactory::GetForProfile(profile_)
          ->GetContentSettingAndMaybeUpdateLastUsage(
              requesting_origin, embedding_origin, content_settings_type(),
              std::string());

  if (push_content_setting == CONTENT_SETTING_BLOCK) {
    DVLOG(1) << "Push permission was explicitly blocked.";
    PermissionUmaUtil::PermissionDenied(permission_type(), requesting_origin);
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        true /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  if (notification_status == blink::mojom::PermissionStatus::DENIED) {
    DVLOG(1) << "Notification permission has not been granted.";
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  PermissionUmaUtil::PermissionGranted(permission_type(), requesting_origin);
  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      true /* persist */, CONTENT_SETTING_ALLOW);
}
