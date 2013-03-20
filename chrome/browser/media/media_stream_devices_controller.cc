// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_controller.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"

using content::BrowserThread;

namespace {

// Return true if a microphone device is availbale.
bool HasMicrophoneDevice() {
  const content::MediaStreamDevices& audio_devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();

  return !audio_devices.empty();
}

// Return true if a camera device is availbale.
bool HasCameraDevice() {
  const content::MediaStreamDevices& video_devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();
  return !video_devices.empty();
}

// Store a permission setting in the given |host_content_settings_map|.
void SetPermission(HostContentSettingsMap* host_content_settings_map,
                   const GURL& request_origin,
                   ContentSettingsType content_type,
                   ContentSetting content_setting) {
  // Storing mediastream permission settings on Android is not supported yet.
#if !defined(OS_ANDROID)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(request_origin);

  // Check the pattern is valid and don't store settings for invalid patterns.
  // When the request is from a file access, no exception will be made because
  // the pattern will be invalid.
  // FIXME: Add an explicit check for file URLs instead of relying on this
  // implicit behavior. If this is just a preconditiono, then change it to a
  // DCHECK.
  if (!primary_pattern.IsValid())
   return;

  host_content_settings_map->SetContentSetting(
       primary_pattern,
       ContentSettingsPattern::Wildcard(),
       content_type,
       std::string(),
       content_setting);
#endif
}

// Returns the permission settings for the given |request_origin| and
// |content_type|. If the policy with the given |policy_name| is set, then the
// setting determined by the policy is returned.
// TODO(pastramovj): Implement proper policy support for camera and microphone
// policies and replace the |Profile| dependency with a dependency on the
// |HostContentSettingsMap|.
ContentSetting GetPermission(Profile* profile,
                             const GURL& request_origin,
                             ContentSettingsType content_type,
                             const char* policy_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  PrefService* prefs = profile->GetPrefs();
  if (prefs->IsManagedPreference(policy_name))
    return prefs->GetBoolean(policy_name) ? CONTENT_SETTING_ALLOW
                                          : CONTENT_SETTING_BLOCK;
  ContentSetting setting =
      profile->GetHostContentSettingsMap()->GetContentSetting(
          request_origin, request_origin, content_type, NO_RESOURCE_IDENTIFIER);
  // For legacey reasons the content settings page (chrome://settings/content)
  // does not allow to configure default values for the content settigs types
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC and
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA yet. Instead the default setting
  // of the content settings type CONTENT_SETTINGS_TYPE_MEDIASTREAM is used.
  // Page specific permission settings for the content types
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA are set either to
  // CONTENT_SETTING_ALLOW or the CONTENT_SETTING_BLOCK. If no page specific
  // setting fot the given |request_origin| exists, then the hardcoded default
  // value CONTENT_SETTING_ASK is returned. In this case the default setting of
  // content settings type CONTENT_SETTINGS_TYPE_MEDIASTREAM must be fetched.
  // TODO(markusheintz): Change the code to use the default settings for the
  // mic and camera content settings.
  if (setting == CONTENT_SETTING_ASK) {
    setting = profile->GetHostContentSettingsMap()->GetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM, NULL);
  }

  return setting;
}

}  // namespace

MediaStreamDevicesController::MediaStreamDevicesController(
    Profile* profile,
    TabSpecificContentSettings* content_settings,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : profile_(profile),
      content_settings_(content_settings),
      request_(request),
      callback_(callback),
      microphone_access_(MEDIA_ACCESS_NOT_REQUESTED),
      camera_access_(MEDIA_ACCESS_NOT_REQUESTED) {
}

MediaStreamDevicesController::~MediaStreamDevicesController() {}

// static
void MediaStreamDevicesController::RegisterUserPrefs(
    PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed,
                             true,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed,
                             true,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool MediaStreamDevicesController::ProcessRequest() {
  // The requests with type tab audio and tab video cature are requests for
  // screen casting.
  if (request_.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      request_.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
    DCHECK_NE(request_.audio_type, content::MEDIA_DEVICE_AUDIO_CAPTURE);
    DCHECK_NE(request_.video_type, content::MEDIA_DEVICE_VIDEO_CAPTURE);
    DCHECK_NE(request_.request_type, content::MEDIA_OPEN_DEVICE);
    return ProcessScreenCaptureRequest();
  }
  return ProcessMicrophoneCameraRequest();
}

bool MediaStreamDevicesController::ProcessScreenCaptureRequest() {
#if defined(OS_ANDROID)
  microphone_access_ = MEDIA_ACCESS_BLOCKED;
  camera_access_ = MEDIA_ACCESS_BLOCKED;
  callback_.Run(content::MediaStreamDevices());
#else
  // For tab media requests, we need to make sure the request came from the
  // extension API, so we check the registry here.
  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile_);

  if (!registry->VerifyRequest(request_.render_process_id,
                               request_.render_view_id)) {
    microphone_access_ = MEDIA_ACCESS_BLOCKED;
    camera_access_ = MEDIA_ACCESS_BLOCKED;
    callback_.Run(content::MediaStreamDevices());
  } else {
    content::MediaStreamDevices devices;
    if (request_.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_TAB_AUDIO_CAPTURE, "", ""));
    }
    if (request_.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_TAB_VIDEO_CAPTURE, "", ""));
    }

    callback_.Run(devices);
  }
#endif

  return true;
}

bool MediaStreamDevicesController::ProcessMicrophoneCameraRequest() {
  // Chrome polices for camera and microphone access are mapped to camera and
  // microphone permission settings. If policies for camera and microphone
  // access are set, then the camera and microphone permission settings are
  // controlled by the policies. In order to respect policies for camera and
  // microphone access, the camera and microphone settings must be fetched
  // first.
  if (request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
    microphone_access_ = ContentSettingToMediaAccess(
        GetPermission(
          profile_,
          GetRequestOrigin(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          prefs::kAudioCaptureAllowed));
  }

  if (request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
    camera_access_ = ContentSettingToMediaAccess(
        GetPermission(
          profile_,
          GetRequestOrigin(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          prefs::kVideoCaptureAllowed));
  }

  // Requests with type MEDIA_OPEN_DEVICE are send from flapper. Flapper has a
  // separate permissions UI and manages it's own page specific permissions
  // similar to content settings. Flapper camera and microphone permissions are
  // checked before this code is run. If microphone and or camera access was
  // denied, then this code is not called. Therefor it is save to change media
  // stream access from MEDIA_ACCESS_REQUESTED to MEDIA_ACCESS_ALLOWED.
  // Changing media stream access from MEDIA_ACCESS_REQUESTED to
  // MEDIA_ACCESS_ALLOWED prevents prompting the users for permissions again.
  // IMPORTANT: Policies for camera and microphone access are integrated into
  // the content settings sytem. These policies don't support
  // CONTENT_SETTING_ASK as permission value. Is means media stream access
  // can't be set to MEDIA_ACCESS_REQUESTED via policy. But media stream
  // settings set to MEDIA_ACCESS_BLOCK or MEDIA_ACCESS_ALLOW must not be
  // changed as they could have been set by policy.
  if (request_.request_type == content::MEDIA_OPEN_DEVICE) {
    if (microphone_access_ == MEDIA_ACCESS_REQUESTED)
      microphone_access_ = MEDIA_ACCESS_BLOCKED;
    if (camera_access_ == MEDIA_ACCESS_REQUESTED)
      camera_access_ = MEDIA_ACCESS_BLOCKED;
    CompleteProcessingRequest();
    return true;
  }

  // Block camera and microphone access for empty request origins. Request
  // origins can be empty when file URLS are used without the
  // |--allow-file-access-from-files| flag.
  if (GetRequestOrigin().is_empty()) {
    microphone_access_ = MEDIA_ACCESS_BLOCKED;
    camera_access_ = MEDIA_ACCESS_BLOCKED;
    CompleteProcessingRequest();
    return true;
  }

  if (camera_access_ != MEDIA_ACCESS_REQUESTED &&
      microphone_access_ != MEDIA_ACCESS_REQUESTED) {
    CompleteProcessingRequest();
    return true;
  }

  // Based on the current permission settings the user must be asked to grant
  // or deny the requested media stream access.
  return false;
}

bool MediaStreamDevicesController::IsMicrophoneRequested() const {
  return microphone_access_ == MEDIA_ACCESS_REQUESTED;
}

bool MediaStreamDevicesController::IsCameraRequested() const {
  return camera_access_ == MEDIA_ACCESS_REQUESTED;
}

const GURL& MediaStreamDevicesController::GetRequestOrigin() const {
  return request_.security_origin;
}

void MediaStreamDevicesController::OnGrantPermission() {
  if (microphone_access_ == MEDIA_ACCESS_REQUESTED) {
    microphone_access_ = MEDIA_ACCESS_ALLOWED;
    if (GetRequestOrigin().SchemeIsSecure()) {
      SetPermission(profile_->GetHostContentSettingsMap(),
                    GetRequestOrigin(),
                    CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                    CONTENT_SETTING_ALLOW);
    }
  }
  if (camera_access_ == MEDIA_ACCESS_REQUESTED) {
    camera_access_ = MEDIA_ACCESS_ALLOWED;
    if (GetRequestOrigin().SchemeIsSecure()) {
      SetPermission(profile_->GetHostContentSettingsMap(),
                    GetRequestOrigin(),
                    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                    CONTENT_SETTING_ALLOW);
    }
  }

  CompleteProcessingRequest();
}

void MediaStreamDevicesController::OnDenyPermission() {
  if (microphone_access_ == MEDIA_ACCESS_REQUESTED) {
    microphone_access_ = MEDIA_ACCESS_BLOCKED;
    SetPermission(profile_->GetHostContentSettingsMap(),
                  GetRequestOrigin(),
                  CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                  CONTENT_SETTING_BLOCK);
  }
  if (camera_access_ == MEDIA_ACCESS_REQUESTED) {
    camera_access_ = MEDIA_ACCESS_BLOCKED;
    SetPermission(profile_->GetHostContentSettingsMap(),
                  GetRequestOrigin(),
                  CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                  CONTENT_SETTING_BLOCK);
  }

  CompleteProcessingRequest();
}

void MediaStreamDevicesController::OnCancel() {
  if (microphone_access_ == MEDIA_ACCESS_REQUESTED)
    microphone_access_ = MEDIA_ACCESS_BLOCKED;
  if (camera_access_ == MEDIA_ACCESS_REQUESTED)
    camera_access_ = MEDIA_ACCESS_BLOCKED;
  CompleteProcessingRequest();
}

void MediaStreamDevicesController::CompleteProcessingRequest() {
  DCHECK_NE(microphone_access_, MEDIA_ACCESS_REQUESTED);
  DCHECK_NE(camera_access_, MEDIA_ACCESS_REQUESTED);

  if (!HasMicrophoneDevice())
    microphone_access_ = MEDIA_ACCESS_NO_DEVICE;
  if (!HasCameraDevice())
    camera_access_ = MEDIA_ACCESS_NO_DEVICE;

  if (content_settings_) {
    if (microphone_access_ == MEDIA_ACCESS_BLOCKED)
      content_settings_->OnMicrophoneAccessBlocked();
    else if (microphone_access_ == MEDIA_ACCESS_ALLOWED)
      content_settings_->OnMicrophoneAccessed();

    if (camera_access_ == MEDIA_ACCESS_BLOCKED)
      content_settings_->OnCameraAccessBlocked();
    else if (camera_access_ == MEDIA_ACCESS_ALLOWED)
      content_settings_->OnCameraAccessed();

    // TODO(markusheintz): Remove this work argound once the
    // TabSpecificContentSettings, the ContentSettingsImage and the
    // ContentSettingsBubbleModel are changed.
    if (microphone_access_ == MEDIA_ACCESS_BLOCKED ||
        camera_access_ == MEDIA_ACCESS_BLOCKED) {
      content_settings_->OnContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                                          std::string());
    } else if (microphone_access_ == MEDIA_ACCESS_ALLOWED ||
               camera_access_ == MEDIA_ACCESS_ALLOWED) {
      content_settings_->OnMediaStreamAllowed();
    }
  }

  bool get_microphone_stream = microphone_access_ == MEDIA_ACCESS_ALLOWED;
  bool get_camera_stream = camera_access_ == MEDIA_ACCESS_ALLOWED;

  if (!get_microphone_stream && !get_camera_stream) {
    callback_.Run(content::MediaStreamDevices());
    return;
  }

  content::MediaStreamDevices devices;
  switch (request_.request_type) {
    case content::MEDIA_OPEN_DEVICE:
      // For open device request pick the desired device or fall back to the
      // first available of the given type.
      MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request_.requested_device_id,
          get_microphone_stream,
          get_camera_stream,
          &devices);
      break;
    case content::MEDIA_DEVICE_ACCESS:
    case content::MEDIA_GENERATE_STREAM:
    case content::MEDIA_ENUMERATE_DEVICES:
      // Get the default devices for the request.
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetDefaultDevicesForProfile(profile_,
                                      get_microphone_stream,
                                      get_camera_stream,
                                      &devices);
      break;
  }
  callback_.Run(devices);
}

MediaStreamDevicesController::MediaAccess
MediaStreamDevicesController::ContentSettingToMediaAccess(
    ContentSetting setting) const {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return MEDIA_ACCESS_ALLOWED;
    case CONTENT_SETTING_BLOCK:
      return MEDIA_ACCESS_BLOCKED;
    case CONTENT_SETTING_ASK:
      return MEDIA_ACCESS_REQUESTED;
    default:
      NOTREACHED();
  }

  return MEDIA_ACCESS_NOT_REQUESTED;
}
