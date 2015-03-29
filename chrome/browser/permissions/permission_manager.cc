// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include "base/callback.h"
#include "chrome/browser/content_settings/permission_context_base.h"
#include "chrome/browser/permissions/permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace {

// Helper method to convert ContentSetting to content::PermissionStatus.
content::PermissionStatus
ContentSettingToPermissionStatus(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_SESSION_ONLY:
      return content::PERMISSION_STATUS_GRANTED;
    case CONTENT_SETTING_BLOCK:
      return content::PERMISSION_STATUS_DENIED;
    case CONTENT_SETTING_ASK:
      return content::PERMISSION_STATUS_ASK;
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return content::PERMISSION_STATUS_DENIED;
}

// Helper method to convert content::PermissionType to ContentSettingType.
ContentSettingsType PermissionTypeToContentSetting(
    content::PermissionType permission) {
  switch (permission) {
    case content::PermissionType::MIDI_SYSEX:
      return CONTENT_SETTINGS_TYPE_MIDI_SYSEX;
    case content::PermissionType::PUSH_MESSAGING:
      return CONTENT_SETTINGS_TYPE_PUSH_MESSAGING;
    case content::PermissionType::NOTIFICATIONS:
      return CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
    case content::PermissionType::GEOLOCATION:
      return CONTENT_SETTINGS_TYPE_GEOLOCATION;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
      return CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER;
#else
      NOTIMPLEMENTED();
      break;
#endif
    case content::PermissionType::NUM:
      // This will hit the NOTREACHED below.
      break;
  }

  NOTREACHED() << "Unknown content setting for permission "
               << static_cast<int>(permission);
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

// Helper method that wraps a callback a void(content::PermissionStatus)
// callback into a void(ContentSetting) callback.
void PermissionStatusCallbackWrapper(
    const base::Callback<void(content::PermissionStatus)>& callback,
    ContentSetting content_setting) {
  callback.Run(ContentSettingToPermissionStatus(content_setting));
}

}  // anonymous namespace

PermissionManager::PermissionManager(Profile* profile)
    : profile_(profile) {
}

PermissionManager::~PermissionManager() {
}

void PermissionManager::RequestPermission(
    content::PermissionType permission,
    content::WebContents* web_contents,
    int request_id,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context) {
    callback.Run(content::PERMISSION_STATUS_DENIED);
    return;
  }

  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  const PermissionRequestID request(render_process_id,
                                    render_view_id,
                                    request_id,
                                    requesting_origin);

  context->RequestPermission(web_contents, request, requesting_origin,
                             user_gesture,
                             base::Bind(&PermissionStatusCallbackWrapper,
                                        callback));
}

void PermissionManager::CancelPermissionRequest(
    content::PermissionType permission,
    content::WebContents* web_contents,
    int request_id,
    const GURL& requesting_origin) {
  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context)
    return;

  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  const PermissionRequestID request(render_process_id,
                                    render_view_id,
                                    request_id,
                                    requesting_origin);

  context->CancelPermissionRequest(web_contents, request);
}

void PermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context)
    return;

  context->ResetPermission(requesting_origin.GetOrigin(),
                           embedding_origin.GetOrigin());
}

content::PermissionStatus PermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context)
    return content::PERMISSION_STATUS_DENIED;

  return ContentSettingToPermissionStatus(
      context->GetPermissionStatus(requesting_origin.GetOrigin(),
                                   embedding_origin.GetOrigin()));
}

void PermissionManager::RegisterPermissionUsage(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  profile_->GetHostContentSettingsMap()->UpdateLastUsage(
      requesting_origin,
      embedding_origin,
      PermissionTypeToContentSetting(permission));
}
