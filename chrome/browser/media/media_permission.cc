// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_permission.h"

#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_device_permission_context.h"
#include "chrome/browser/media/media_stream_device_permissions.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

namespace {

content::PermissionType ContentSettingsTypeToPermission(
    ContentSettingsType content_setting) {
  if (content_setting == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    return content::PermissionType::AUDIO_CAPTURE;
  } else {
    DCHECK_EQ(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, content_setting);
    return content::PermissionType::VIDEO_CAPTURE;
  }
}

}  // namespace

MediaPermission::MediaPermission(ContentSettingsType content_type,
                                 bool is_insecure_pepper_request,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin,
                                 Profile* profile)
    : content_type_(content_type),
      is_insecure_pepper_request_(is_insecure_pepper_request),
      requesting_origin_(requesting_origin),
      embedding_origin_(embedding_origin),
      profile_(profile) {}

ContentSetting MediaPermission::GetPermissionStatus(
    content::MediaStreamRequestResult* denial_reason) const {
  // Deny the request if the security origin is empty, this happens with
  // file access without |--allow-file-access-from-files| flag.
  if (requesting_origin_.is_empty()) {
    *denial_reason = content::MEDIA_DEVICE_INVALID_SECURITY_ORIGIN;
    return CONTENT_SETTING_BLOCK;
  }

  // Use the Permission Context to find out if the kill switch is on. Set the
  // denial reason to kill switch.
  content::PermissionType permission_type =
      ContentSettingsTypeToPermission(content_type_);
  // TODO(raymes): This calls into GetPermissionContext which is a private
  // member of PermissionManager. Remove this call when this class is refactored
  // into a PermissionContext. See crbug.com/596786.
  PermissionContextBase* permission_context =
      PermissionManager::Get(profile_)->GetPermissionContext(permission_type);

  if (!permission_context) {
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }

  MediaStreamDevicePermissionContext* media_device_permission_context =
      static_cast<MediaStreamDevicePermissionContext*>(permission_context);

  if (media_device_permission_context->IsPermissionKillSwitchOn()) {
    *denial_reason = content::MEDIA_DEVICE_KILL_SWITCH_ON;
    return CONTENT_SETTING_BLOCK;
  }

  // Check policy and content settings.
  ContentSetting result =
      GetStoredContentSetting(media_device_permission_context);
  if (result == CONTENT_SETTING_BLOCK)
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;

  return result;
}

ContentSetting MediaPermission::GetPermissionStatusWithDeviceRequired(
    const std::string& device_id,
    content::MediaStreamRequestResult* denial_reason) const {
  // Deny the request if there is no device attached to the OS of the requested
  // type.
  if (!HasAvailableDevices(device_id)) {
    *denial_reason = content::MEDIA_DEVICE_NO_HARDWARE;
    return CONTENT_SETTING_BLOCK;
  }

  return GetPermissionStatus(denial_reason);
}

ContentSetting MediaPermission::GetStoredContentSetting(
    MediaStreamDevicePermissionContext* media_device_permission_context) const {
  if (is_insecure_pepper_request_) {
    return media_device_permission_context
        ->GetPermissionStatusAllowingInsecureForPepper(requesting_origin_,
                                                       embedding_origin_);
  } else {
    return media_device_permission_context->GetPermissionStatus(
        requesting_origin_, embedding_origin_);
  }
}

bool MediaPermission::HasAvailableDevices(const std::string& device_id) const {
  const content::MediaStreamDevices* devices = nullptr;
  if (content_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    devices =
        &MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    devices =
        &MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();
  } else {
    NOTREACHED();
  }

  // TODO(tommi): It's kind of strange to have this here since if we fail this
  // test, there'll be a UI shown that indicates to the user that access to
  // non-existing audio/video devices has been denied.  The user won't have
  // any way to change that but there will be a UI shown which indicates that
  // access is blocked.
  if (devices->empty())
    return false;

  // Note: we check device_id before dereferencing devices. If the requested
  // device id is non-empty, then the corresponding device list must not be
  // NULL.
  if (!device_id.empty() && !devices->FindById(device_id))
    return false;

  return true;
}
