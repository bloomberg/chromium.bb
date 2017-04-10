// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_permission.h"

#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

MediaPermission::MediaPermission(ContentSettingsType content_type,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin,
                                 Profile* profile,
                                 content::WebContents* web_contents)
    : content_type_(content_type),
      requesting_origin_(requesting_origin),
      embedding_origin_(embedding_origin),
      profile_(profile),
      web_contents_(web_contents) {
  // Currently |web_contents_| is only used on ChromeOS but it's not worth
  // #ifdef'ing out all its usage, so just mark it used here.
  (void)web_contents_;
}

ContentSetting MediaPermission::GetPermissionStatus(
    content::MediaStreamRequestResult* denial_reason) const {
  DCHECK(!requesting_origin_.is_empty());

  PermissionManager* permission_manager = PermissionManager::Get(profile_);

  // Find out if the kill switch is on. Set the denial reason to kill switch.
  if (permission_manager->IsPermissionKillSwitchOn(content_type_)) {
    *denial_reason = content::MEDIA_DEVICE_KILL_SWITCH_ON;
    return CONTENT_SETTING_BLOCK;
  }

  // Check policy and content settings.
  ContentSetting content_setting =
      permission_manager
          ->GetPermissionStatus(content_type_, requesting_origin_,
                                embedding_origin_)
          .content_setting;
  if (content_setting == CONTENT_SETTING_BLOCK)
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
  return content_setting;
}
