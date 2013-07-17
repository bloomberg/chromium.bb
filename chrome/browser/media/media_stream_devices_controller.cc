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
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using content::BrowserThread;

namespace {

bool HasAnyAvailableDevice() {
  const content::MediaStreamDevices& audio_devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
  const content::MediaStreamDevices& video_devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();

  return !audio_devices.empty() || !video_devices.empty();
}

bool IsInKioskMode() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return true;

#if defined(OS_CHROMEOS)
  const chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  return user_manager && user_manager->IsLoggedInAsKioskApp();
#else
  return false;
#endif
}

}  // namespace

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : web_contents_(web_contents),
      request_(request),
      callback_(callback),
      // For MEDIA_OPEN_DEVICE requests (Pepper) we always request both webcam
      // and microphone to avoid popping two infobars.
      microphone_requested_(
          request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
          request.request_type == content::MEDIA_OPEN_DEVICE),
      webcam_requested_(
          request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
          request.request_type == content::MEDIA_OPEN_DEVICE) {
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content_settings_ = TabSpecificContentSettings::FromWebContents(web_contents);

  // Don't call GetDevicePolicy from the initializer list since the
  // implementation depends on member variables.
  if (microphone_requested_ &&
      GetDevicePolicy(prefs::kAudioCaptureAllowed,
                      prefs::kAudioCaptureAllowedUrls) == ALWAYS_DENY) {
    microphone_requested_ = false;
  }

  if (webcam_requested_ &&
      GetDevicePolicy(prefs::kVideoCaptureAllowed,
                      prefs::kVideoCaptureAllowedUrls) == ALWAYS_DENY) {
    webcam_requested_ = false;
  }
}

MediaStreamDevicesController::~MediaStreamDevicesController() {
  if (!callback_.is_null()) {
    callback_.Run(content::MediaStreamDevices(),
                  scoped_ptr<content::MediaStreamUI>());
  }
}

// static
void MediaStreamDevicesController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed,
                             true,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed,
                             true,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kVideoCaptureAllowedUrls,
                          user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kAudioCaptureAllowedUrls,
                          user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}


bool MediaStreamDevicesController::DismissInfoBarAndTakeActionOnSettings() {
  // Tab capture is allowed for extensions only and infobar is not shown for
  // extensions.
  if (request_.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      request_.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
    Deny(false);
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

  // Filter any parts of the request that have been blocked by default and deny
  // it if nothing is left to accept.
  if (FilterBlockedByDefaultDevices() == 0) {
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
  NotifyUIRequestAccepted();

  // Get the default devices for the request.
  content::MediaStreamDevices devices;
  if (microphone_requested_ || webcam_requested_) {
    switch (request_.request_type) {
      case content::MEDIA_OPEN_DEVICE: {
        const content::MediaStreamDevice* device = NULL;
        // For open device request pick the desired device or fall back to the
        // first available of the given type.
        if (request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
          device = MediaCaptureDevicesDispatcher::GetInstance()->
              GetRequestedAudioDevice(request_.requested_audio_device_id);
          // TODO(wjia): Confirm this is the intended behavior.
          if (!device) {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetFirstAvailableAudioDevice();
          }
        } else if (request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
          // Pepper API opens only one device at a time.
          device = MediaCaptureDevicesDispatcher::GetInstance()->
              GetRequestedVideoDevice(request_.requested_video_device_id);
          // TODO(wjia): Confirm this is the intended behavior.
          if (!device) {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetFirstAvailableVideoDevice();
          }
        }
        if (device)
          devices.push_back(*device);
        break;
      } case content::MEDIA_GENERATE_STREAM: {
        bool needs_audio_device = microphone_requested_;
        bool needs_video_device = webcam_requested_;

        // Get the exact audio or video device if an id is specified.
        if (!request_.requested_audio_device_id.empty()) {
          const content::MediaStreamDevice* audio_device =
              MediaCaptureDevicesDispatcher::GetInstance()->
                  GetRequestedAudioDevice(request_.requested_audio_device_id);
          if (audio_device) {
            devices.push_back(*audio_device);
            needs_audio_device = false;
          }
        }
        if (!request_.requested_video_device_id.empty()) {
          const content::MediaStreamDevice* video_device =
              MediaCaptureDevicesDispatcher::GetInstance()->
                  GetRequestedVideoDevice(request_.requested_video_device_id);
          if (video_device) {
            devices.push_back(*video_device);
            needs_video_device = false;
          }
        }

        // If either or both audio and video devices were requested but not
        // specified by id, get the default devices.
        if (needs_audio_device || needs_video_device) {
          MediaCaptureDevicesDispatcher::GetInstance()->
              GetDefaultDevicesForProfile(profile_,
                                          needs_audio_device,
                                          needs_video_device,
                                          &devices);
        }
        break;
      } case content::MEDIA_DEVICE_ACCESS:
        // Get the default devices for the request.
        MediaCaptureDevicesDispatcher::GetInstance()->
            GetDefaultDevicesForProfile(profile_,
                                        microphone_requested_,
                                        webcam_requested_,
                                        &devices);
        break;
      case content::MEDIA_ENUMERATE_DEVICES:
        // Do nothing.
        NOTREACHED();
        break;
    }

    // TODO(raymes): We currently set the content permission for non-https
    // websites for Pepper requests as well. This is temporary and should be
    // removed.
    if (update_content_setting) {
      if ((IsSchemeSecure() && !devices.empty()) ||
          request_.request_type == content::MEDIA_OPEN_DEVICE) {
        SetPermission(true);
      }
    }
  }

  scoped_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()->
        GetMediaStreamCaptureIndicator()->RegisterMediaStream(
            web_contents_, devices);
  }
  content::MediaResponseCallback cb = callback_;
  callback_.Reset();
  cb.Run(devices, ui.Pass());
}

void MediaStreamDevicesController::Deny(bool update_content_setting) {
  NotifyUIRequestDenied();

  if (update_content_setting)
    SetPermission(false);

  content::MediaResponseCallback cb = callback_;
  callback_.Reset();
  cb.Run(content::MediaStreamDevices(), scoped_ptr<content::MediaStreamUI>());
}

MediaStreamDevicesController::DevicePolicy
MediaStreamDevicesController::GetDevicePolicy(
    const char* policy_name,
    const char* whitelist_policy_name) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the security origin policy matches a value in the whitelist, allow it.
  // Otherwise, check the |policy_name| master switch for the default behavior.

  PrefService* prefs = profile_->GetPrefs();

  // TODO(tommi): Remove the kiosk mode check when the whitelist below
  // is visible in the media exceptions UI.
  // See discussion here: https://codereview.chromium.org/15738004/
  if (IsInKioskMode()) {
    const base::ListValue* list = prefs->GetList(whitelist_policy_name);
    std::string value;
    for (size_t i = 0; i < list->GetSize(); ++i) {
      if (list->GetString(i, &value)) {
        ContentSettingsPattern pattern =
            ContentSettingsPattern::FromString(value);
        if (pattern == ContentSettingsPattern::Wildcard()) {
          DLOG(WARNING) << "Ignoring wildcard URL pattern: " << value;
          continue;
        }
        DLOG_IF(ERROR, !pattern.IsValid()) << "Invalid URL pattern: " << value;
        if (pattern.IsValid() && pattern.Matches(request_.security_origin))
          return ALWAYS_ALLOW;
      }
    }
  }

  // If a match was not found, check if audio capture is otherwise disallowed
  // or if the user should be prompted.  Setting the policy value to "true"
  // is equal to not setting it at all, so from hereon out, we will return
  // either POLICY_NOT_SET (prompt) or ALWAYS_DENY (no prompt, no access).
  if (!prefs->GetBoolean(policy_name))
    return ALWAYS_DENY;

  return POLICY_NOT_SET;
}

bool MediaStreamDevicesController::IsRequestAllowedByDefault() const {
  // The request from internal objects like chrome://URLs is always allowed.
  if (ShouldAlwaysAllowOrigin())
    return true;

  struct {
    bool has_capability;
    const char* policy_name;
    const char* list_policy_name;
    ContentSettingsType settings_type;
  } device_checks[] = {
    { microphone_requested_, prefs::kAudioCaptureAllowed,
      prefs::kAudioCaptureAllowedUrls, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC },
    { webcam_requested_, prefs::kVideoCaptureAllowed,
      prefs::kVideoCaptureAllowedUrls,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(device_checks); ++i) {
    if (!device_checks[i].has_capability)
      continue;

    DevicePolicy policy = GetDevicePolicy(device_checks[i].policy_name,
                                          device_checks[i].list_policy_name);

    if (policy == ALWAYS_DENY)
      return false;

    if (policy == POLICY_NOT_SET) {
      // Only load content settings from secure origins unless it is a
      // content::MEDIA_OPEN_DEVICE (Pepper) request.
      if (!IsSchemeSecure() &&
          request_.request_type != content::MEDIA_OPEN_DEVICE) {
        return false;
      }
      if (profile_->GetHostContentSettingsMap()->GetContentSetting(
              request_.security_origin, request_.security_origin,
              device_checks[i].settings_type, NO_RESOURCE_IDENTIFIER) !=
              CONTENT_SETTING_ALLOW) {
        return false;
      }
    }
    // If we get here, then either policy is set to ALWAYS_ALLOW or the content
    // settings allow the request by default.
  }

  return true;
}

int MediaStreamDevicesController::FilterBlockedByDefaultDevices() {
  int requested_devices = microphone_requested_ + webcam_requested_;
  if (microphone_requested_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          NO_RESOURCE_IDENTIFIER) == CONTENT_SETTING_BLOCK) {
    requested_devices--;
    microphone_requested_ = false;
  }

  if (webcam_requested_ &&
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
          NO_RESOURCE_IDENTIFIER) == CONTENT_SETTING_BLOCK) {
    requested_devices--;
    webcam_requested_ = false;
  }

  return requested_devices;
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
#if defined(OS_ANDROID)
  // We do not support sticky operations on Android yet.
  return;
#endif
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

void MediaStreamDevicesController::NotifyUIRequestAccepted() const {
  if (!content_settings_)
    return;

  // We need to figure out which part of the request is accepted or denied here.
  // For example, when the request contains both audio and video, but audio is
  // blocked by the policy, then we will prompt the infobar to ask for video
  // permission. In case the users approve the permission,
  // we need to show an allowed icon for video but blocked icon for audio.
  if (request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
    // The request might contain audio while |webcam_requested_| is false,
    // this happens when the policy is blocking the audio.
    if (microphone_requested_)
      content_settings_->OnMicrophoneAccessed();
    else
      content_settings_->OnMicrophoneAccessBlocked();
  }

  if (request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
    // The request might contain video while |webcam_requested_| is false,
    // this happens when the policy is blocking the video.
    if (webcam_requested_)
      content_settings_->OnCameraAccessed();
    else
      content_settings_->OnCameraAccessBlocked();
  }
}

void MediaStreamDevicesController::NotifyUIRequestDenied() const {
  if (!content_settings_)
    return;

  // Do not show the block icons for tab capture.
  if (request_.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      request_.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      return;
  }

  if (request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE)
    content_settings_->OnMicrophoneAccessBlocked();
  if (request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE)
    content_settings_->OnCameraAccessBlocked();
}
