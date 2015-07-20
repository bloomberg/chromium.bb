// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_permission.h"

#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

MediaPermission::MediaPermission(ContentSettingsType content_type,
                                 content::MediaStreamRequestType request_type,
                                 const GURL& origin,
                                 const std::string& device_id,
                                 Profile* profile)
    : content_type_(content_type),
      request_type_(request_type),
      origin_(origin),
      device_id_(device_id),
      profile_(profile) {
}

ContentSetting MediaPermission::GetPermissionStatus(
    content::MediaStreamRequestResult* denial_reason) const {
  // Deny the request if the security origin is empty, this happens with
  // file access without |--allow-file-access-from-files| flag.
  if (origin_.is_empty()) {
    *denial_reason = content::MEDIA_DEVICE_INVALID_SECURITY_ORIGIN;
    return CONTENT_SETTING_BLOCK;
  }

  // Deny the request if there is no device attached to the OS of the requested
  // type.
  if (!HasAvailableDevices()) {
    *denial_reason = content::MEDIA_DEVICE_NO_HARDWARE;
    return CONTENT_SETTING_BLOCK;
  }

  // Check policy and content settings.
  ContentSetting result = GetStoredContentSetting();
  if (result == CONTENT_SETTING_BLOCK)
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;

  return result;
}

ContentSetting MediaPermission::GetStoredContentSetting() const {
  // TODO(raymes): Merge this policy check into content settings
  // crbug.com/244389.
  const char* policy_name = nullptr;
  const char* urls_policy_name = nullptr;
  if (content_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    policy_name = prefs::kAudioCaptureAllowed;
    urls_policy_name = prefs::kAudioCaptureAllowedUrls;
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    policy_name = prefs::kVideoCaptureAllowed;
    urls_policy_name = prefs::kVideoCaptureAllowedUrls;
  } else {
    NOTREACHED();
  }

  MediaStreamDevicePolicy policy =
      GetDevicePolicy(profile_, origin_, policy_name, urls_policy_name);

  if (policy == ALWAYS_DENY)
    return CONTENT_SETTING_BLOCK;

  if (policy == ALWAYS_ALLOW)
    return CONTENT_SETTING_ALLOW;

  DCHECK(policy == POLICY_NOT_SET);
  // Check the content setting.
  ContentSetting setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          origin_, origin_, content_type_,
          content_settings::ResourceIdentifier());

  if (setting == CONTENT_SETTING_DEFAULT)
    return CONTENT_SETTING_ASK;

  // TODO(raymes): This is here for safety to ensure that we always ask the user
  // even if a content setting is set to "allow" if the origin is insecure. In
  // reality we shouldn't really need to check this here as we should respect
  // the user's content setting. The problem is that pepper requests allow
  // insecure origins to be persisted. We should stop allowing this, do some
  // sort of migration and remove this check.
  if (!ShouldPersistContentSetting(setting, origin_, request_type_) &&
      !origin_.SchemeIs(extensions::kExtensionScheme) &&
      !origin_.SchemeIs(content::kChromeUIScheme) &&
      !origin_.SchemeIs(content::kChromeDevToolsScheme)) {
    return CONTENT_SETTING_ASK;
  }

  return setting;
}

bool MediaPermission::HasAvailableDevices() const {
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
  if (!device_id_.empty() && !devices->FindById(device_id_))
    return false;

  return true;
}
