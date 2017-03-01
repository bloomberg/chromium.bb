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
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#endif

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
  // Deny the request if the security origin is empty, this happens with
  // file access without |--allow-file-access-from-files| flag.
  if (requesting_origin_.is_empty()) {
    *denial_reason = content::MEDIA_DEVICE_INVALID_SECURITY_ORIGIN;
    return CONTENT_SETTING_BLOCK;
  }

  PermissionManager* permission_manager = PermissionManager::Get(profile_);

  // Find out if the kill switch is on. Set the denial reason to kill switch.
  if (permission_manager->IsPermissionKillSwitchOn(content_type_)) {
    *denial_reason = content::MEDIA_DEVICE_KILL_SWITCH_ON;
    return CONTENT_SETTING_BLOCK;
  }

#if defined(OS_CHROMEOS)
  // Special permissions if the request is coming from a ChromeOS login page.
  chromeos::LoginDisplayHost* login_display_host =
      chromeos::LoginDisplayHost::default_host();
  chromeos::WebUILoginView* webui_login_view =
      login_display_host ? login_display_host->GetWebUILoginView() : nullptr;
  content::WebContents* login_web_contents =
      webui_login_view ? webui_login_view->GetWebContents() : nullptr;
  if (web_contents_ == login_web_contents) {
    if (content_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
      *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
      return CONTENT_SETTING_BLOCK;
    }

    const chromeos::CrosSettings* const settings =
        chromeos::CrosSettings::Get();
    if (!settings) {
      *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
      return CONTENT_SETTING_BLOCK;
    }

    const base::Value* const raw_list_value =
        settings->GetPref(chromeos::kLoginVideoCaptureAllowedUrls);
    if (!raw_list_value) {
      *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
      return CONTENT_SETTING_BLOCK;
    }

    const base::ListValue* list_value;
    const bool is_list = raw_list_value->GetAsList(&list_value);
    DCHECK(is_list);
    for (const auto& base_value : *list_value) {
      std::string value;
      if (base_value->GetAsString(&value)) {
        const ContentSettingsPattern pattern =
            ContentSettingsPattern::FromString(value);
        if (pattern == ContentSettingsPattern::Wildcard()) {
          LOG(WARNING) << "Ignoring wildcard URL pattern: " << value;
          continue;
        }
        if (pattern.IsValid() && pattern.Matches(requesting_origin_))
          return CONTENT_SETTING_ALLOW;
      }
    }

    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }
#endif  // defined(OS_CHROMEOS)

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
