// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_permission.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

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
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin,
                                 Profile* profile)
    : content_type_(content_type),
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

  content::PermissionType permission_type =
      ContentSettingsTypeToPermission(content_type_);
  PermissionManager* permission_manager = PermissionManager::Get(profile_);

  // Find out if the kill switch is on. Set the denial reason to kill switch.
  if (permission_manager->IsPermissionKillSwitchOn(permission_type)) {
    *denial_reason = content::MEDIA_DEVICE_KILL_SWITCH_ON;
    return CONTENT_SETTING_BLOCK;
  }

  // Check policy and content settings.
  blink::mojom::PermissionStatus status =
      permission_manager->GetPermissionStatus(
          permission_type, requesting_origin_, embedding_origin_);
  switch (status) {
    case blink::mojom::PermissionStatus::DENIED:
      *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
      return CONTENT_SETTING_BLOCK;
    case blink::mojom::PermissionStatus::ASK:
      return CONTENT_SETTING_ASK;
    case blink::mojom::PermissionStatus::GRANTED:
      return CONTENT_SETTING_ALLOW;
  }

  NOTREACHED();
  return CONTENT_SETTING_BLOCK;
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
