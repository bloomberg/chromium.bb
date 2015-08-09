// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_controller.h"

#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_permission.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/media_stream_request.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "content/public/browser/android/content_view_core.h"
#include "ui/android/window_android.h"
#endif  // OS_ANDROID

using content::BrowserThread;

namespace {

enum DevicePermissionActions {
  kAllowHttps = 0,
  kAllowHttp,
  kDeny,
  kCancel,
  kPermissionActionsMax  // Must always be last!
};

// Returns true if the given ContentSettingsType is being requested in
// |request|.
bool ContentTypeIsRequested(ContentSettingsType type,
                            const content::MediaStreamRequest& request) {
  if (request.request_type == content::MEDIA_OPEN_DEVICE)
    return true;

  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    return request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE;

  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
    return request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE;

  return false;
}

}  // namespace

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : web_contents_(web_contents),
      request_(request),
      callback_(callback) {
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content_settings_ = TabSpecificContentSettings::FromWebContents(web_contents);

  content::MediaStreamRequestResult denial_reason = content::MEDIA_DEVICE_OK;
  old_audio_setting_ = GetContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                         request_, &denial_reason);
  old_video_setting_ = GetContentSetting(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, request_, &denial_reason);

  // If either setting is ask, we show the infobar.
  if (old_audio_setting_ == CONTENT_SETTING_ASK ||
      old_video_setting_ == CONTENT_SETTING_ASK) {
    return;
  }

  // Otherwise we can run the callback immediately.
  RunCallback(old_audio_setting_, old_video_setting_, denial_reason);
}

MediaStreamDevicesController::~MediaStreamDevicesController() {
  if (!callback_.is_null()) {
    callback_.Run(content::MediaStreamDevices(),
                  content::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN,
                  scoped_ptr<content::MediaStreamUI>());
  }
}

// static
void MediaStreamDevicesController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed, true);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed, true);
  prefs->RegisterListPref(prefs::kVideoCaptureAllowedUrls);
  prefs->RegisterListPref(prefs::kAudioCaptureAllowedUrls);
}

bool MediaStreamDevicesController::IsAskingForAudio() const {
  return old_audio_setting_ == CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::IsAskingForVideo() const {
  return old_video_setting_ == CONTENT_SETTING_ASK;
}

const std::string& MediaStreamDevicesController::GetSecurityOriginSpec() const {
  return request_.security_origin.spec();
}

int MediaStreamDevicesController::GetIconID() const {
  if (IsAskingForVideo())
    return IDR_INFOBAR_MEDIA_STREAM_CAMERA;

  return IDR_INFOBAR_MEDIA_STREAM_MIC;
}

base::string16 MediaStreamDevicesController::GetMessageText() const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
  if (!IsAskingForAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY;
  else if (!IsAskingForVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY;
  return l10n_util::GetStringFUTF16(
      message_id, base::UTF8ToUTF16(GetSecurityOriginSpec()));
}

base::string16 MediaStreamDevicesController::GetMessageTextFragment() const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_PERMISSION_FRAGMENT;
  if (!IsAskingForAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
  else if (!IsAskingForVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
  return l10n_util::GetStringUTF16(message_id);
}

bool MediaStreamDevicesController::HasUserGesture() const {
  return request_.user_gesture;
}

GURL MediaStreamDevicesController::GetRequestingHostname() const {
  return request_.security_origin;
}

void MediaStreamDevicesController::PermissionGranted() {
  GURL origin(GetSecurityOriginSpec());
  if (origin.SchemeIsSecure()) {
    UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                              kAllowHttps, kPermissionActionsMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                              kAllowHttp, kPermissionActionsMax);
  }
  RunCallback(GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                            old_audio_setting_, CONTENT_SETTING_ALLOW),
              GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                            old_video_setting_, CONTENT_SETTING_ALLOW),
              content::MEDIA_DEVICE_PERMISSION_DENIED);
}

void MediaStreamDevicesController::PermissionDenied() {
  UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                            kDeny, kPermissionActionsMax);
  RunCallback(GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                            old_audio_setting_, CONTENT_SETTING_BLOCK),
              GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                            old_video_setting_, CONTENT_SETTING_BLOCK),
              content::MEDIA_DEVICE_PERMISSION_DENIED);
}

void MediaStreamDevicesController::Cancelled() {
  UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                            kCancel, kPermissionActionsMax);
  RunCallback(old_audio_setting_, old_video_setting_,
              content::MEDIA_DEVICE_PERMISSION_DISMISSED);
}

void MediaStreamDevicesController::RequestFinished() {
  delete this;
}

content::MediaStreamDevices MediaStreamDevicesController::GetDevices(
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  bool audio_allowed = audio_setting == CONTENT_SETTING_ALLOW;
  bool video_allowed = video_setting == CONTENT_SETTING_ALLOW;

  if (!audio_allowed && !video_allowed)
    return content::MediaStreamDevices();

  content::MediaStreamDevices devices;
  switch (request_.request_type) {
    case content::MEDIA_OPEN_DEVICE: {
      const content::MediaStreamDevice* device = NULL;
      // For open device request, when requested device_id is empty, pick
      // the first available of the given type. If requested device_id is
      // not empty, return the desired device if it's available. Otherwise,
      // return no device.
      if (audio_allowed &&
          request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
        DCHECK_EQ(content::MEDIA_NO_SERVICE, request_.video_type);
        if (!request_.requested_audio_device_id.empty()) {
          device =
              MediaCaptureDevicesDispatcher::GetInstance()
                  ->GetRequestedAudioDevice(request_.requested_audio_device_id);
        } else {
          device = MediaCaptureDevicesDispatcher::GetInstance()
                       ->GetFirstAvailableAudioDevice();
        }
      } else if (video_allowed &&
                 request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
        DCHECK_EQ(content::MEDIA_NO_SERVICE, request_.audio_type);
        // Pepper API opens only one device at a time.
        if (!request_.requested_video_device_id.empty()) {
          device =
              MediaCaptureDevicesDispatcher::GetInstance()
                  ->GetRequestedVideoDevice(request_.requested_video_device_id);
        } else {
          device = MediaCaptureDevicesDispatcher::GetInstance()
                       ->GetFirstAvailableVideoDevice();
        }
      }
      if (device)
        devices.push_back(*device);
      break;
    }
    case content::MEDIA_GENERATE_STREAM: {
      bool get_default_audio_device = audio_allowed;
      bool get_default_video_device = video_allowed;

      // Get the exact audio or video device if an id is specified.
      if (audio_allowed && !request_.requested_audio_device_id.empty()) {
        const content::MediaStreamDevice* audio_device =
            MediaCaptureDevicesDispatcher::GetInstance()
                ->GetRequestedAudioDevice(request_.requested_audio_device_id);
        if (audio_device) {
          devices.push_back(*audio_device);
          get_default_audio_device = false;
        }
      }
      if (video_allowed && !request_.requested_video_device_id.empty()) {
        const content::MediaStreamDevice* video_device =
            MediaCaptureDevicesDispatcher::GetInstance()
                ->GetRequestedVideoDevice(request_.requested_video_device_id);
        if (video_device) {
          devices.push_back(*video_device);
          get_default_video_device = false;
        }
      }

      // If either or both audio and video devices were requested but not
      // specified by id, get the default devices.
      if (get_default_audio_device || get_default_video_device) {
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetDefaultDevicesForProfile(profile_, get_default_audio_device,
                                          get_default_video_device, &devices);
      }
      break;
    }
    case content::MEDIA_DEVICE_ACCESS: {
      // Get the default devices for the request.
      MediaCaptureDevicesDispatcher::GetInstance()->GetDefaultDevicesForProfile(
          profile_, audio_allowed, video_allowed, &devices);
      break;
    }
    case content::MEDIA_ENUMERATE_DEVICES: {
      // Do nothing.
      NOTREACHED();
      break;
    }
  }  // switch

  if (audio_allowed) {
    profile_->GetHostContentSettingsMap()->UpdateLastUsageByPattern(
        ContentSettingsPattern::FromURLNoWildcard(request_.security_origin),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  }
  if (video_allowed) {
    profile_->GetHostContentSettingsMap()->UpdateLastUsageByPattern(
        ContentSettingsPattern::FromURLNoWildcard(request_.security_origin),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  }

  return devices;
}

void MediaStreamDevicesController::RunCallback(
    ContentSetting audio_setting,
    ContentSetting video_setting,
    content::MediaStreamRequestResult denial_reason) {
  StorePermission(audio_setting, video_setting);
  UpdateTabSpecificContentSettings(audio_setting, video_setting);

  content::MediaStreamDevices devices =
      GetDevices(audio_setting, video_setting);

  // If either audio or video are allowed then the callback should report
  // success, otherwise we report |denial_reason|.
  content::MediaStreamRequestResult request_result = content::MEDIA_DEVICE_OK;
  if (audio_setting != CONTENT_SETTING_ALLOW &&
      video_setting != CONTENT_SETTING_ALLOW) {
    DCHECK_NE(content::MEDIA_DEVICE_OK, denial_reason);
    request_result = denial_reason;
  } else if (devices.empty()) {
    // Even if one of the content settings was allowed, if there are no devices
    // at this point we still report a failure.
    request_result = content::MEDIA_DEVICE_NO_HARDWARE;
  }

  scoped_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents_, devices);
  }
  content::MediaResponseCallback cb = callback_;
  callback_.Reset();
  cb.Run(devices, request_result, ui.Pass());
}

void MediaStreamDevicesController::StorePermission(
    ContentSetting new_audio_setting,
    ContentSetting new_video_setting) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(request_.security_origin);

  if (IsAskingForAudio() && new_audio_setting != CONTENT_SETTING_ASK) {
    if (ShouldPersistContentSetting(new_audio_setting, request_.security_origin,
                                    request_.request_type)) {
      profile_->GetHostContentSettingsMap()->SetContentSetting(
          primary_pattern, ContentSettingsPattern::Wildcard(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(),
          new_audio_setting);
    }
  }
  if (IsAskingForVideo() && new_video_setting != CONTENT_SETTING_ASK) {
    if (ShouldPersistContentSetting(new_video_setting, request_.security_origin,
                                    request_.request_type)) {
      profile_->GetHostContentSettingsMap()->SetContentSetting(
          primary_pattern, ContentSettingsPattern::Wildcard(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(),
          new_video_setting);
    }
  }
}

void MediaStreamDevicesController::UpdateTabSpecificContentSettings(
    ContentSetting audio_setting,
    ContentSetting video_setting) const {
  if (!content_settings_)
    return;

  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;
  std::string selected_audio_device;
  std::string selected_video_device;
  std::string requested_audio_device = request_.requested_audio_device_id;
  std::string requested_video_device = request_.requested_video_device_id;

  // TODO(raymes): Why do we use the defaults here for the selected devices?
  // Shouldn't we just use the devices that were actually selected?
  PrefService* prefs = Profile::FromBrowserContext(
                           web_contents_->GetBrowserContext())->GetPrefs();
  if (audio_setting != CONTENT_SETTING_DEFAULT) {
    selected_audio_device =
        requested_audio_device.empty()
            ? prefs->GetString(prefs::kDefaultAudioCaptureDevice)
            : requested_audio_device;
    microphone_camera_state |=
        TabSpecificContentSettings::MICROPHONE_ACCESSED |
        (audio_setting == CONTENT_SETTING_ALLOW
             ? 0
             : TabSpecificContentSettings::MICROPHONE_BLOCKED);
  }

  if (video_setting != CONTENT_SETTING_DEFAULT) {
    selected_video_device =
        requested_video_device.empty()
            ? prefs->GetString(prefs::kDefaultVideoCaptureDevice)
            : requested_video_device;
    microphone_camera_state |=
        TabSpecificContentSettings::CAMERA_ACCESSED |
        (video_setting == CONTENT_SETTING_ALLOW
             ? 0
             : TabSpecificContentSettings::CAMERA_BLOCKED);
  }

  content_settings_->OnMediaStreamPermissionSet(
      request_.security_origin, microphone_camera_state, selected_audio_device,
      selected_video_device, requested_audio_device, requested_video_device);
}

ContentSetting MediaStreamDevicesController::GetContentSetting(
    ContentSettingsType content_type,
    const content::MediaStreamRequest& request,
    content::MediaStreamRequestResult* denial_reason) const {
  DCHECK(content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  std::string requested_device_id;
  if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    requested_device_id = request.requested_audio_device_id;
  else if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
    requested_device_id = request.requested_video_device_id;

  if (!IsUserAcceptAllowed(content_type)) {
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }

  if (ContentTypeIsRequested(content_type, request)) {
    MediaPermission permission(content_type, request.request_type,
                               request.security_origin, requested_device_id,
                               profile_);
    return permission.GetPermissionStatus(denial_reason);
  }
  // Return the default content setting if the device is not requested.
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting MediaStreamDevicesController::GetNewSetting(
    ContentSettingsType content_type,
    ContentSetting old_setting,
    ContentSetting user_decision) const {
  DCHECK(user_decision == CONTENT_SETTING_ALLOW ||
         user_decision == CONTENT_SETTING_BLOCK);
  ContentSetting result = old_setting;
  if (old_setting == CONTENT_SETTING_ASK) {
    if (user_decision == CONTENT_SETTING_ALLOW &&
        IsUserAcceptAllowed(content_type)) {
      result = CONTENT_SETTING_ALLOW;
    } else if (user_decision == CONTENT_SETTING_BLOCK) {
      result = CONTENT_SETTING_BLOCK;
    }
  }
  return result;
}

bool MediaStreamDevicesController::IsUserAcceptAllowed(
    ContentSettingsType content_type) const {
#if defined(OS_ANDROID)
  content::ContentViewCore* cvc =
      content::ContentViewCore::FromWebContents(web_contents_);
  if (!cvc)
    return false;

  ui::WindowAndroid* window_android = cvc->GetWindowAndroid();
  if (!window_android)
    return false;

  std::string android_permission =
      PrefServiceBridge::GetAndroidPermissionForContentSetting(content_type);
  bool android_permission_blocked = false;
  if (!android_permission.empty()) {
    android_permission_blocked =
        !window_android->HasPermission(android_permission) &&
        !window_android->CanRequestPermission(android_permission);
  }
  if (android_permission_blocked)
    return false;

  // Don't approve device requests if the tab was hidden.
  // TODO(qinmin): Add a test for this. http://crbug.com/396869.
  // TODO(raymes): Shouldn't this apply to all permissions not just audio/video?
  return web_contents_->GetRenderWidgetHostView()->IsShowing();
#endif
  return true;
}
