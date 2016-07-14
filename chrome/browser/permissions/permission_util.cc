// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_util.h"

#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"

using content::PermissionType;

std::size_t PermissionTypeHash::operator()(
    const content::PermissionType& type) const {
  return static_cast<size_t>(type);
}

// The returned strings must match the RAPPOR metrics in rappor.xml,
// and any Field Trial configs for the Permissions kill switch e.g.
// Permissions.Action.Geolocation etc..
std::string PermissionUtil::GetPermissionString(
    content::PermissionType permission) {
  switch (permission) {
    case content::PermissionType::GEOLOCATION:
      return "Geolocation";
    case content::PermissionType::NOTIFICATIONS:
      return "Notifications";
    case content::PermissionType::MIDI_SYSEX:
      return "MidiSysEx";
    case content::PermissionType::PUSH_MESSAGING:
      return "PushMessaging";
    case content::PermissionType::DURABLE_STORAGE:
      return "DurableStorage";
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case content::PermissionType::AUDIO_CAPTURE:
      return "AudioCapture";
    case content::PermissionType::VIDEO_CAPTURE:
      return "VideoCapture";
    case content::PermissionType::MIDI:
      return "Midi";
    case content::PermissionType::BACKGROUND_SYNC:
      return "BackgroundSync";
    case content::PermissionType::NUM:
      break;
  }
  NOTREACHED();
  return std::string();
}

bool PermissionUtil::GetPermissionType(ContentSettingsType type,
                                       PermissionType* out) {
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    *out = PermissionType::GEOLOCATION;
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    *out = PermissionType::NOTIFICATIONS;
  } else if (type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    *out = PermissionType::MIDI_SYSEX;
  } else if (type == CONTENT_SETTINGS_TYPE_PUSH_MESSAGING) {
    *out = PermissionType::PUSH_MESSAGING;
  } else if (type == CONTENT_SETTINGS_TYPE_DURABLE_STORAGE) {
    *out = PermissionType::DURABLE_STORAGE;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    *out = PermissionType::VIDEO_CAPTURE;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    *out = PermissionType::AUDIO_CAPTURE;
  } else if (type == CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC) {
    *out = PermissionType::BACKGROUND_SYNC;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  } else if (type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
    *out = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
#endif
  } else {
    return false;
  }
  return true;
}

void PermissionUtil::SetContentSettingAndRecordRevocation(
    Profile* profile,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    std::string resource_identifier,
    ContentSetting setting) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting previous_value = map->GetContentSetting(
      primary_url, secondary_url, content_type, resource_identifier);

  map->SetContentSettingDefaultScope(primary_url, secondary_url, content_type,
                                     resource_identifier, setting);

  ContentSetting final_value = map->GetContentSetting(
      primary_url, secondary_url, content_type, resource_identifier);

  if (previous_value == CONTENT_SETTING_ALLOW &&
      final_value != CONTENT_SETTING_ALLOW) {
    PermissionType permission_type;
    if (PermissionUtil::GetPermissionType(content_type, &permission_type)) {
      PermissionUmaUtil::PermissionRevoked(permission_type, primary_url);
    }
  }
}
