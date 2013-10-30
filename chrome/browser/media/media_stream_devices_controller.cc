// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_controller.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/constants.h"

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

MediaStreamDevicesController::MediaStreamTypeSettings::MediaStreamTypeSettings(
    Permission permission, const std::string& requested_device_id):
    permission(permission), requested_device_id(requested_device_id) {}

MediaStreamDevicesController::MediaStreamTypeSettings::
    MediaStreamTypeSettings(): permission(MEDIA_NONE) {}

MediaStreamDevicesController::MediaStreamTypeSettings::
    ~MediaStreamTypeSettings() {}

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : web_contents_(web_contents),
      request_(request),
      callback_(callback) {
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content_settings_ = TabSpecificContentSettings::FromWebContents(web_contents);

  // For MEDIA_OPEN_DEVICE requests (Pepper) we always request both webcam
  // and microphone to avoid popping two infobars.
  // We start with setting the requested media type to allowed or blocked
  // depending on the policy. If not blocked by policy it may be blocked later
  // in the two remaining filtering steps (by user setting or by user when
  // clicking the infobar).
  // TODO(grunell): It's not the nicest solution to let the MEDIA_OPEN_DEVICE
  // case take a ride on the MEDIA_DEVICE_*_CAPTURE permission. Should be fixed.
  if (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
      request.request_type == content::MEDIA_OPEN_DEVICE) {
    if (GetDevicePolicy(prefs::kAudioCaptureAllowed,
                        prefs::kAudioCaptureAllowedUrls) == ALWAYS_DENY) {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_AUDIO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_BLOCKED_BY_POLICY,
                                  request.requested_audio_device_id)));
    } else {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_AUDIO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_ALLOWED,
                                  request.requested_audio_device_id)));
    }
  }
  if (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
      request.request_type == content::MEDIA_OPEN_DEVICE) {
    if (GetDevicePolicy(prefs::kVideoCaptureAllowed,
                        prefs::kVideoCaptureAllowedUrls) == ALWAYS_DENY) {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_VIDEO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_BLOCKED_BY_POLICY,
                                  request.requested_video_device_id)));
    } else {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_VIDEO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_ALLOWED,
                                  request.requested_video_device_id)));
    }
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

  if (request_.request_type == content::MEDIA_OPEN_DEVICE) {
    bool no_matched_audio_device =
        (request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE &&
         !request_.requested_audio_device_id.empty() &&
         MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedAudioDevice(
             request_.requested_audio_device_id) == NULL);
    bool no_matched_video_device =
        (request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE &&
         !request_.requested_video_device_id.empty() &&
         MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedVideoDevice(
             request_.requested_video_device_id) == NULL);
    if (no_matched_audio_device || no_matched_video_device) {
      Deny(false);
      return true;
    }
  }

  // Show the infobar.
  return false;
}

bool MediaStreamDevicesController::HasAudio() const {
  return IsDeviceAudioCaptureRequestedAndAllowed();
}

bool MediaStreamDevicesController::HasVideo() const {
  return IsDeviceVideoCaptureRequestedAndAllowed();
}

const std::string& MediaStreamDevicesController::GetSecurityOriginSpec() const {
  return request_.security_origin.spec();
}

void MediaStreamDevicesController::Accept(bool update_content_setting) {
  NotifyUIRequestAccepted();

  // Get the default devices for the request.
  content::MediaStreamDevices devices;
  bool audio_allowed = IsDeviceAudioCaptureRequestedAndAllowed();
  bool video_allowed = IsDeviceVideoCaptureRequestedAndAllowed();
  if (audio_allowed || video_allowed) {
    switch (request_.request_type) {
      case content::MEDIA_OPEN_DEVICE: {
        const content::MediaStreamDevice* device = NULL;
        // For open device request, when requested device_id is empty, pick
        // the first available of the given type. If requested device_id is
        // not empty, return the desired device if it's available. Otherwise,
        // return no device.
        if (audio_allowed &&
            request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
          if (!request_.requested_audio_device_id.empty()) {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetRequestedAudioDevice(request_.requested_audio_device_id);
          } else {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetFirstAvailableAudioDevice();
          }
        } else if (video_allowed &&
            request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
          // Pepper API opens only one device at a time.
          if (!request_.requested_video_device_id.empty()) {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetRequestedVideoDevice(request_.requested_video_device_id);
          } else {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetFirstAvailableVideoDevice();
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
              MediaCaptureDevicesDispatcher::GetInstance()->
                  GetRequestedAudioDevice(request_.requested_audio_device_id);
          if (audio_device) {
            devices.push_back(*audio_device);
            get_default_audio_device = false;
          }
        }
        if (video_allowed && !request_.requested_video_device_id.empty()) {
          const content::MediaStreamDevice* video_device =
              MediaCaptureDevicesDispatcher::GetInstance()->
                  GetRequestedVideoDevice(request_.requested_video_device_id);
          if (video_device) {
            devices.push_back(*video_device);
            get_default_video_device = false;
          }
        }

        // If either or both audio and video devices were requested but not
        // specified by id, get the default devices.
        if (get_default_audio_device || get_default_video_device) {
          MediaCaptureDevicesDispatcher::GetInstance()->
              GetDefaultDevicesForProfile(profile_,
                                          get_default_audio_device,
                                          get_default_video_device,
                                          &devices);
        }
        break;
      }
      case content::MEDIA_DEVICE_ACCESS: {
        // Get the default devices for the request.
        MediaCaptureDevicesDispatcher::GetInstance()->
            GetDefaultDevicesForProfile(profile_,
                                        audio_allowed,
                                        video_allowed,
                                        &devices);
        break;
      }
      case content::MEDIA_ENUMERATE_DEVICES: {
        // Do nothing.
        NOTREACHED();
        break;
      }
    }  // switch

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
    { IsDeviceAudioCaptureRequestedAndAllowed(), prefs::kAudioCaptureAllowed,
      prefs::kAudioCaptureAllowedUrls, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC },
    { IsDeviceVideoCaptureRequestedAndAllowed(), prefs::kVideoCaptureAllowed,
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
  int requested_devices = 0;

  if (IsDeviceAudioCaptureRequestedAndAllowed()) {
    if (profile_->GetHostContentSettingsMap()->GetContentSetting(
        request_.security_origin,
        request_.security_origin,
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        NO_RESOURCE_IDENTIFIER) == CONTENT_SETTING_BLOCK) {
      request_permissions_[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
          MEDIA_BLOCKED_BY_USER_SETTING;
    } else {
      ++requested_devices;
    }
  }

  if (IsDeviceVideoCaptureRequestedAndAllowed()) {
    if (profile_->GetHostContentSettingsMap()->GetContentSetting(
        request_.security_origin,
        request_.security_origin,
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
        NO_RESOURCE_IDENTIFIER) == CONTENT_SETTING_BLOCK) {
      request_permissions_[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
          MEDIA_BLOCKED_BY_USER_SETTING;
    } else {
      ++requested_devices;
    }
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
  return request_.security_origin.SchemeIsSecure() ||
      request_.security_origin.SchemeIs(extensions::kExtensionScheme) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableUserMediaSecurity);
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
  if (request_permissions_.find(content::MEDIA_DEVICE_AUDIO_CAPTURE) !=
      request_permissions_.end()) {
      profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        std::string(),
        content_setting);
  }
  if (request_permissions_.find(content::MEDIA_DEVICE_VIDEO_CAPTURE) !=
      request_permissions_.end()) {
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

  content_settings_->OnMediaStreamPermissionSet(request_.security_origin,
                                                request_permissions_);
}

void MediaStreamDevicesController::NotifyUIRequestDenied() {
  if (!content_settings_)
    return;

  if (IsDeviceAudioCaptureRequestedAndAllowed()) {
    request_permissions_[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
        MEDIA_BLOCKED_BY_USER;
  }
  if (IsDeviceVideoCaptureRequestedAndAllowed()) {
    request_permissions_[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
        MEDIA_BLOCKED_BY_USER;
  }

  content_settings_->OnMediaStreamPermissionSet(request_.security_origin,
                                                request_permissions_);
}

bool MediaStreamDevicesController::IsDeviceAudioCaptureRequestedAndAllowed()
    const {
  MediaStreamTypeSettingsMap::const_iterator it =
      request_permissions_.find(content::MEDIA_DEVICE_AUDIO_CAPTURE);
  return (it != request_permissions_.end() &&
          it->second.permission == MEDIA_ALLOWED);
}

bool MediaStreamDevicesController::IsDeviceVideoCaptureRequestedAndAllowed()
    const {
  MediaStreamTypeSettingsMap::const_iterator it =
      request_permissions_.find(content::MEDIA_DEVICE_VIDEO_CAPTURE);
  return (it != request_permissions_.end() &&
          it->second.permission == MEDIA_ALLOWED);
}
