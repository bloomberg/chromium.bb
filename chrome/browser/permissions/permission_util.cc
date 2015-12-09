// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_util.h"

#include "base/logging.h"
#include "content/public/browser/permission_type.h"

using content::PermissionType;

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
    case content::PermissionType::NUM:
      break;
  }
  NOTREACHED();
  return std::string();
}
