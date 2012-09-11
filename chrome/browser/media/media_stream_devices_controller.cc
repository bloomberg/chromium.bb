// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_controller.h"

#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/media_stream_request.h"

using content::BrowserThread;

namespace {

// A predicate that checks if a StreamDeviceInfo object has the same device
// name as the device name specified at construction.
class DeviceNameEquals {
 public:
  explicit DeviceNameEquals(const std::string& device_name)
      : device_name_(device_name) {
  }

  bool operator() (const content::MediaStreamDevice& device) {
    return device.name == device_name_;
  }

 private:
  std::string device_name_;
};

const char kAudioKey[] = "audio";
const char kVideoKey[] = "video";

}  // namespace

MediaStreamDevicesController::MediaStreamDevicesController(
    Profile* profile,
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback)
    : has_audio_(false),
      has_video_(false),
      profile_(profile),
      request_(*request),
      callback_(callback) {
  for (content::MediaStreamDeviceMap::const_iterator it =
           request_.devices.begin();
       it != request_.devices.end(); ++it) {
    if (content::IsAudioMediaType(it->first)) {
      has_audio_ |= !it->second.empty();
    } else if (content::IsVideoMediaType(it->first)) {
      has_video_ |= !it->second.empty();
    }
  }
}

MediaStreamDevicesController::~MediaStreamDevicesController() {}

bool MediaStreamDevicesController::DismissInfoBarAndTakeActionOnSettings() {
  // Deny the request if the security origin is empty, this happens with
  // file access without |--allow-file-access-from-files| flag.
  if (request_.security_origin.is_empty()) {
    Deny();
    return true;
  }

  // Deny the request and don't show the infobar if there is no devices.
  if (!has_audio_ && !has_video_) {
    // TODO(xians): We should detect this in a early state, and post a callback
    // to tell the users that no device is available. Remove the code and add
    // a DCHECK when this is done.
    Deny();
    return true;
  }

  // Checks if any exception has been made for this request, get the "always
  // allowed" devices if they are available.
  std::string audio, video;
  GetAlwaysAllowedDevices(&audio, &video);
  if ((has_audio_ && audio.empty()) || (has_video_ && video.empty())) {
    // If there is no "always allowed" device for the origin, or the device is
    // not available in the device lists, Check the default setting to see if
    // the user has blocked the access to the media device.
    if (IsMediaDeviceBlocked()) {
      Deny();
      return true;
    }

    // Show the infobar.
    return false;
  }

  // Dismiss the infobar by selecting the "always allowed" devices.
  Accept(audio, video, false);
  return true;
}

content::MediaStreamDevices
MediaStreamDevicesController::GetAudioDevices() const {
  content::MediaStreamDevices all_audio_devices;
  if (has_audio_)
    FindSubsetOfDevices(&content::IsAudioMediaType, &all_audio_devices);
  return all_audio_devices;
}

content::MediaStreamDevices
MediaStreamDevicesController::GetVideoDevices() const {
  content::MediaStreamDevices all_video_devices;
  if (has_video_)
    FindSubsetOfDevices(&content::IsVideoMediaType, &all_video_devices);
  return all_video_devices;
}

const std::string& MediaStreamDevicesController::GetSecurityOriginSpec() const {
  return request_.security_origin.spec();
}

bool MediaStreamDevicesController::IsSafeToAlwaysAllowAudio() const {
  return IsSafeToAlwaysAllow(
      &content::IsAudioMediaType, content::MEDIA_DEVICE_AUDIO_CAPTURE);
}

bool MediaStreamDevicesController::IsSafeToAlwaysAllowVideo() const {
  return IsSafeToAlwaysAllow(
      &content::IsVideoMediaType, content::MEDIA_DEVICE_VIDEO_CAPTURE);
}

void MediaStreamDevicesController::Accept(const std::string& audio_id,
                                          const std::string& video_id,
                                          bool always_allow) {
  content::MediaStreamDevices devices;
  std::string audio_device_name, video_device_name;

  const content::MediaStreamDevice* const audio_device =
      FindFirstDeviceWithIdInSubset(&content::IsAudioMediaType, audio_id);
  if (audio_device) {
    if (audio_device->type != content::MEDIA_DEVICE_AUDIO_CAPTURE)
      always_allow = false;  // Override for virtual audio device type.
    devices.push_back(*audio_device);
    audio_device_name = audio_device->name;
  }

  const content::MediaStreamDevice* const video_device =
      FindFirstDeviceWithIdInSubset(&content::IsVideoMediaType, video_id);
  if (video_device) {
    if (video_device->type != content::MEDIA_DEVICE_VIDEO_CAPTURE)
      always_allow = false;  // Override for virtual video device type.
    devices.push_back(*video_device);
    video_device_name = video_device->name;
  }

  DCHECK(!devices.empty());

  if (always_allow)
    AlwaysAllowOriginAndDevices(audio_device_name, video_device_name);

  callback_.Run(devices);
}

void MediaStreamDevicesController::Deny() {
  callback_.Run(content::MediaStreamDevices());
}

bool MediaStreamDevicesController::IsSafeToAlwaysAllow(
    FilterByDeviceTypeFunc is_included,
    content::MediaStreamDeviceType device_type) const {
  DCHECK(device_type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
         device_type == content::MEDIA_DEVICE_VIDEO_CAPTURE);

  if (!request_.security_origin.SchemeIsSecure())
    return false;

  // If non-physical devices are available for the choosing, then it's not safe.
  bool safe_devices_found = false;
  for (content::MediaStreamDeviceMap::const_iterator it =
           request_.devices.begin();
       it != request_.devices.end(); ++it) {
    if (it->first != device_type && is_included(it->first))
      return false;
    safe_devices_found = true;
  }

  return safe_devices_found;
}

bool MediaStreamDevicesController::ShouldAlwaysAllowOrigin() {
  return profile_->GetHostContentSettingsMap()->ShouldAllowAllContent(
      request_.security_origin, request_.security_origin,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM);
}

bool MediaStreamDevicesController::IsMediaDeviceBlocked() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ContentSetting current_setting =
      profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM, NULL);
  return (current_setting == CONTENT_SETTING_BLOCK);
}

void MediaStreamDevicesController::AlwaysAllowOriginAndDevices(
    const std::string& audio_device,
    const std::string& video_device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!audio_device.empty() || !video_device.empty());
  DictionaryValue* dictionary_value = new DictionaryValue();
  if (!audio_device.empty())
    dictionary_value->SetString(kAudioKey, audio_device);

  if (!video_device.empty())
    dictionary_value->SetString(kVideoKey, video_device);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(request_.security_origin);
  profile_->GetHostContentSettingsMap()->SetWebsiteSetting(
      primary_pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_MEDIASTREAM,
      NO_RESOURCE_IDENTIFIER,
      dictionary_value);
}

void MediaStreamDevicesController::GetAlwaysAllowedDevices(
    std::string* audio_id, std::string* video_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio_id->empty());
  DCHECK(video_id->empty());
  // If the request is from internal objects like chrome://URLs, use the first
  // devices on the lists.
  if (ShouldAlwaysAllowOrigin()) {
    if (has_audio_)
      *audio_id = GetFirstDeviceId(content::MEDIA_DEVICE_AUDIO_CAPTURE);
    if (has_video_)
      *video_id = GetFirstDeviceId(content::MEDIA_DEVICE_VIDEO_CAPTURE);
    return;
  }

  // "Always allowed" option is only available for secure connection.
  if (!request_.security_origin.SchemeIsSecure())
    return;

  // Checks the media exceptions to get the "always allowed" devices.
  scoped_ptr<Value> value(
      profile_->GetHostContentSettingsMap()->GetWebsiteSetting(
          request_.security_origin,
          request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM,
          NO_RESOURCE_IDENTIFIER,
          NULL));
  if (!value.get()) {
    NOTREACHED();
    return;
  }

  const DictionaryValue* value_dict = NULL;
  if (!value->GetAsDictionary(&value_dict) || value_dict->empty())
    return;

  std::string audio_name, video_name;
  value_dict->GetString(kAudioKey, &audio_name);
  value_dict->GetString(kVideoKey, &video_name);

  if (has_audio_ && !audio_name.empty())
    *audio_id = GetDeviceIdByName(content::MEDIA_DEVICE_AUDIO_CAPTURE,
                                  audio_name);
  if (has_video_ && !video_name.empty())
    *video_id = GetDeviceIdByName(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                  video_name);
}

std::string MediaStreamDevicesController::GetDeviceIdByName(
    content::MediaStreamDeviceType type,
    const std::string& name) {
  content::MediaStreamDeviceMap::const_iterator device_it =
      request_.devices.find(type);
  if (device_it != request_.devices.end()) {
    content::MediaStreamDevices::const_iterator it = std::find_if(
        device_it->second.begin(), device_it->second.end(),
        DeviceNameEquals(name));
    if (it != device_it->second.end())
      return it->device_id;
  }

  // Device is not available, return an empty string.
  return std::string();
}

std::string MediaStreamDevicesController::GetFirstDeviceId(
    content::MediaStreamDeviceType type) {
  content::MediaStreamDeviceMap::const_iterator device_it =
      request_.devices.find(type);
  if (device_it != request_.devices.end())
    return device_it->second.begin()->device_id;

  return std::string();
}

void MediaStreamDevicesController::FindSubsetOfDevices(
    FilterByDeviceTypeFunc is_included,
    content::MediaStreamDevices* out) const {
  for (content::MediaStreamDeviceMap::const_iterator it =
           request_.devices.begin();
       it != request_.devices.end(); ++it) {
    if (is_included(it->first))
      out->insert(out->end(), it->second.begin(), it->second.end());
  }
}

const content::MediaStreamDevice*
MediaStreamDevicesController::FindFirstDeviceWithIdInSubset(
    FilterByDeviceTypeFunc is_included,
    const std::string& device_id) const {
  for (content::MediaStreamDeviceMap::const_iterator it =
           request_.devices.begin();
       it != request_.devices.end(); ++it) {
    if (!is_included(it->first)) continue;
    for (content::MediaStreamDevices::const_iterator device_it =
             it->second.begin();
         device_it != it->second.end(); ++device_it) {
      const content::MediaStreamDevice& candidate = *device_it;
      if (candidate.device_id == device_id)
        return &candidate;
    }
  }
  return NULL;
}
