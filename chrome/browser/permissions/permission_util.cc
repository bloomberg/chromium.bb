// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_util.h"

#include "base/logging.h"

// The returned strings must match the RAPPOR metrics in rappor.xml,
// and any Field Trial configs for the Permissions kill switch e.g.
// Permissions.Action.Geolocation etc..
std::string PermissionUtil::GetPermissionString(
    ContentSettingsType permission) {
  switch (permission) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return "Geolocation";
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return "Notifications";
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return "MidiSysEx";
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return "PushMessaging";
    case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
      return "DurableStorage";
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
#endif
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      return "AudioCapture";
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      return "VideoCapture";
    default:
      NOTREACHED();
      return std::string();
  }
}
