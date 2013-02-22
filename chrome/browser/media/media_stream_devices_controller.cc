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
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"

using content::BrowserThread;

namespace {

bool HasAnyAvailableDevice() {
  const content::MediaStreamDevices& audio_devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
  const content::MediaStreamDevices& video_devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();

  return !audio_devices.empty() || !video_devices.empty();
};

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
      microphone_requested_(
          request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE),
      webcam_requested_(
          request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
  // Don't call GetDevicePolicy from the initializer list since the
  // implementation depends on member variables.
  if (microphone_requested_ &&
      GetDevicePolicy(prefs::kAudioCaptureAllowed) == ALWAYS_DENY) {
    microphone_requested_ = false;
  }

  if (webcam_requested_ &&
      GetDevicePolicy(prefs::kVideoCaptureAllowed) == ALWAYS_DENY) {
    webcam_requested_ = false;
  }
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


bool MediaStreamDevicesController::DismissInfoBarAndTakeActionOnSettings() {
  // If this is a no UI check for policies only go straight to accept - policy
  // check will be done automatically on the way.
  if (request_.request_type == content::MEDIA_OPEN_DEVICE) {
    Accept(false);
    return true;
  }

  if (request_.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      request_.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
    HandleTabMediaRequest();
    return true;
  }

  // Deny the request if the security origin is empty, this happens with
  // file access without |--allow-file-access-from-files| flag.
  if (request_.security_origin.is_empty()) {
    Deny(false);
    return true;
  }

  // Deny the request if there is no device attached to the OS.
  if (!HasAnyAvailableDevice()) {
    Deny(false);
    return true;
  }

  // Check if any allow exception has been made for this request.
  if (IsRequestAllowedByDefault()) {
    Accept(false);
    return true;
  }

  // Check if any block exception has been made for this request.
  if (IsRequestBlockedByDefault()) {
    Deny(false);
    return true;
  }

  // Check if the media default setting is set to block.
  if (IsDefaultMediaAccessBlocked()) {
    Deny(false);
    return true;
  }

  // Show the infobar.
  return false;
}

const std::string& MediaStreamDevicesController::GetSecurityOriginSpec() const {
  return request_.security_origin.spec();
}

void MediaStreamDevicesController::Accept(bool update_content_setting) {
  if (content_settings_)
    content_settings_->OnMediaStreamAllowed();

  // Get the default devices for the request.
  content::MediaStreamDevices devices;
  if (microphone_requested_ || webcam_requested_) {
    switch (request_.request_type) {
      case content::MEDIA_OPEN_DEVICE:
        // For open device request pick the desired device or fall back to the
        // first available of the given type.
        MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
            request_.requested_device_id,
            microphone_requested_,
            webcam_requested_,
            &devices);
        break;
      case content::MEDIA_DEVICE_ACCESS:
      case content::MEDIA_GENERATE_STREAM:
      case content::MEDIA_ENUMERATE_DEVICES:
        // Get the default devices for the request.
        MediaCaptureDevicesDispatcher::GetInstance()->
            GetDefaultDevicesForProfile(profile_,
                                        microphone_requested_,
                                        webcam_requested_,
                                        &devices);
        break;
    }

    if (update_content_setting && IsSchemeSecure() && !devices.empty())
      SetPermission(true);
  }

  callback_.Run(devices);
}

void MediaStreamDevicesController::Deny(bool update_content_setting) {
  // TODO(markusheintz): Replace CONTENT_SETTINGS_TYPE_MEDIA_STREAM with the
  // appropriate new CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC and
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA.
  if (content_settings_) {
    content_settings_->OnContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                                        std::string());
  }

  if (update_content_setting)
    SetPermission(false);

  callback_.Run(content::MediaStreamDevices());
}

MediaStreamDevicesController::DevicePolicy
MediaStreamDevicesController::GetDevicePolicy(const char* policy_name) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->IsManagedPreference(policy_name))
    return POLICY_NOT_SET;

  return prefs->GetBoolean(policy_name) ? ALWAYS_ALLOW : ALWAYS_DENY;
}

bool MediaStreamDevicesController::IsRequestAllowedByDefault() const {
  // The request from internal objects like chrome://URLs is always allowed.
  if (ShouldAlwaysAllowOrigin())
    return true;

  struct {
    bool has_capability;
    const char* policy_name;
    ContentSettingsType settings_type;
  } device_checks[] = {
    { microphone_requested_, prefs::kAudioCaptureAllowed,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC },
    { webcam_requested_, prefs::kVideoCaptureAllowed,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(device_checks); ++i) {
    if (!device_checks[i].has_capability)
      continue;

    DevicePolicy policy = GetDevicePolicy(device_checks[i].policy_name);
    if (policy == ALWAYS_DENY ||
        (policy == POLICY_NOT_SET &&
         profile_->GetHostContentSettingsMap()->GetContentSetting(
            request_.security_origin, request_.security_origin,
            device_checks[i].settings_type, NO_RESOURCE_IDENTIFIER) !=
         CONTENT_SETTING_ALLOW)) {
      return false;
    }
    // If we get here, then either policy is set to ALWAYS_ALLOW or the content
    // settings allow the request by default.
  }

  return true;
}

bool MediaStreamDevicesController::IsRequestBlockedByDefault() const {
  if (microphone_requested_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          NO_RESOURCE_IDENTIFIER) != CONTENT_SETTING_BLOCK) {
    return false;
  }

  if (webcam_requested_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          NO_RESOURCE_IDENTIFIER) != CONTENT_SETTING_BLOCK) {
    return false;
  }

  return true;
}

bool MediaStreamDevicesController::IsDefaultMediaAccessBlocked() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(markusheintz): Replace CONTENT_SETTINGS_TYPE_MEDIA_STREAM with the
  // appropriate new CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC and
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA.
  ContentSetting current_setting =
      profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM, NULL);
  return (current_setting == CONTENT_SETTING_BLOCK);
}

void MediaStreamDevicesController::HandleTabMediaRequest() {
#if defined(OS_ANDROID)
  Deny(false);
#else
  // For tab media requests, we need to make sure the request came from the
  // extension API, so we check the registry here.
  extensions::TabCaptureRegistry* registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile_);

  if (!registry->VerifyRequest(request_.render_process_id,
                               request_.render_view_id)) {
    Deny(false);
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
}

bool MediaStreamDevicesController::IsSchemeSecure() const {
  return (request_.security_origin.SchemeIsSecure());
}

bool MediaStreamDevicesController::ShouldAlwaysAllowOrigin() const {
  // TODO(markusheintz): Replace CONTENT_SETTINGS_TYPE_MEDIA_STREAM with the
  // appropriate new CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC and
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA.
  return profile_->GetHostContentSettingsMap()->ShouldAllowAllContent(
      request_.security_origin, request_.security_origin,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM);
}

void MediaStreamDevicesController::SetPermission(bool allowed) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(request_.security_origin);
  // Check the pattern is valid or not. When the request is from a file access,
  // no exception will be made.
  if (!primary_pattern.IsValid())
    return;

  ContentSetting content_setting = allowed ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  if (microphone_requested_) {
      profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        std::string(),
        content_setting);
  }
  if (webcam_requested_) {
    profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
        std::string(),
        content_setting);
  }
}
