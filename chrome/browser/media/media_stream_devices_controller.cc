// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_controller.h"

#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"

using content::BrowserThread;

namespace {

bool HasAnyAvailableDevice() {
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaInternals::GetInstance()->GetMediaCaptureDevicesDispatcher();
  const content::MediaStreamDevices& audio_devices =
      dispatcher->GetAudioCaptureDevices();
  const content::MediaStreamDevices& video_devices =
      dispatcher->GetVideoCaptureDevices();

  return !audio_devices.empty() || !video_devices.empty();
};

const char kAudioKey[] = "audio";
const char kVideoKey[] = "video";

}  // namespace

MediaStreamDevicesController::MediaStreamDevicesController(
    Profile* profile,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : profile_(profile),
      request_(request),
      callback_(callback),
      has_audio_(content::IsAudioMediaType(request.audio_type) &&
                 !IsAudioDeviceBlockedByPolicy()),
      has_video_(content::IsVideoMediaType(request.video_type) &&
                 !IsVideoDeviceBlockedByPolicy()) {
}

MediaStreamDevicesController::~MediaStreamDevicesController() {}

// static
void MediaStreamDevicesController::RegisterUserPrefs(
    PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
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
    HandleTapMediaRequest();
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
  content::MediaStreamDevices devices;
  // If policy has blocked access to some device we might end up with both
  // |has_audio_| and |has_video_| being false in which case we should behave as
  // if Deny(false) was called.
  if (has_audio_ || has_video_) {
    // Get the default devices for the request.
    media::GetDefaultDevicesForProfile(profile_,
                                       has_audio_,
                                       has_video_,
                                       &devices);

    if (update_content_setting && IsSchemeSecure() && !devices.empty())
      SetPermission(true);
  }

  callback_.Run(devices);
}

void MediaStreamDevicesController::Deny(bool update_content_setting) {
  if (update_content_setting)
    SetPermission(false);

  callback_.Run(content::MediaStreamDevices());
}

bool MediaStreamDevicesController::IsAudioDeviceBlockedByPolicy() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return (!profile_->GetPrefs()->GetBoolean(prefs::kAudioCaptureAllowed) &&
          profile_->GetPrefs()->IsManagedPreference(
              prefs::kAudioCaptureAllowed));
}

bool MediaStreamDevicesController::IsVideoDeviceBlockedByPolicy() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return (!profile_->GetPrefs()->GetBoolean(prefs::kVideoCaptureAllowed) &&
          profile_->GetPrefs()->IsManagedPreference(
              prefs::kVideoCaptureAllowed));
}

bool MediaStreamDevicesController::IsRequestAllowedByDefault() const {
  // The request from internal objects like chrome://URLs is always allowed.
  if (ShouldAlwaysAllowOrigin())
    return true;

  if (has_audio_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          NO_RESOURCE_IDENTIFIER) != CONTENT_SETTING_ALLOW) {
    return false;
  }

  if (has_video_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          NO_RESOURCE_IDENTIFIER) != CONTENT_SETTING_ALLOW) {
    return false;
  }

  return true;
}

bool MediaStreamDevicesController::IsRequestBlockedByDefault() const {
  if (has_audio_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          NO_RESOURCE_IDENTIFIER) != CONTENT_SETTING_BLOCK) {
    return false;
  }

  if (has_video_ &&
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
  ContentSetting current_setting =
      profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM, NULL);
  return (current_setting == CONTENT_SETTING_BLOCK);
}

void MediaStreamDevicesController::HandleTapMediaRequest() {
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
}

bool MediaStreamDevicesController::IsSchemeSecure() const {
  return (request_.security_origin.SchemeIsSecure());
}

bool MediaStreamDevicesController::ShouldAlwaysAllowOrigin() const {
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
  if (has_audio_) {
    profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        std::string(),
        content_setting);
  }
  if (has_video_) {
    profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
        std::string(),
        content_setting);
  }
}
