// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram.h"
#include "chrome/browser/content_settings/permission_context_uma_util.h"

namespace {

// Enum for UMA purposes, make sure you update histograms.xml if you add new
// permission actions. Never delete or reorder an entry; only add new entries
// immediately before PERMISSION_NUM
enum PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,

  // Always keep this at the end.
  PERMISSION_ACTION_NUM,
};

// Enum for UMA purposes, make sure you update histograms.xml if you add new
// permission actions. Never delete or reorder an entry; only add new entries
// immediately before PERMISSION_NUM
enum PermissionType {
  PERMISSION_UNKNOWN = 0,
  PERMISSION_MIDI_SYSEX = 1,
  PERMISSION_PUSH_MESSAGING = 2,
  PERMISSION_NOTIFICATIONS = 3,

  // Always keep this at the end.
  PERMISSION_NUM,
};

void RecordPermissionAction(
      ContentSettingsType permission, PermissionAction action) {
  switch (permission) {
      case CONTENT_SETTINGS_TYPE_GEOLOCATION:
        // TODO(miguelg): support geolocation through
        // the generic permission class.
        break;
      case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
        UMA_HISTOGRAM_ENUMERATION(
            "ContentSettings.PermisionActions_Notifications",
            action,
            PERMISSION_ACTION_NUM);
        break;
      case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
        UMA_HISTOGRAM_ENUMERATION("ContentSettings.PermisionActions_MidiSysEx",
                                  action,
                                  PERMISSION_ACTION_NUM);
        break;
      case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
        UMA_HISTOGRAM_ENUMERATION(
            "ContentSettings.PermisionActions_PushMessaging",
            action,
            PERMISSION_ACTION_NUM);
        break;
#if defined(OS_ANDROID)
      case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
        // TODO(miguelg): support protected media through
        // the generic permission class.
        break;
#endif
      default:
        NOTREACHED() << "PERMISSION " << permission << " not accounted for";
    }
}

void RecordPermissionRequest(
    ContentSettingsType permission) {
  PermissionType type;
  switch (permission) {
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      type = PERMISSION_NOTIFICATIONS;
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      type = PERMISSION_MIDI_SYSEX;
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      type = PERMISSION_PUSH_MESSAGING;
      break;
    default:
      NOTREACHED() << "PERMISSION " << permission << " not accounted for";
      type = PERMISSION_UNKNOWN;
  }
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.PermissionRequested", type,
                            PERMISSION_NUM);
}

} // namespace

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionContextUmaUtil::PermissionRequested(
    ContentSettingsType permission) {
  RecordPermissionRequest(permission);
}

void PermissionContextUmaUtil::PermissionGranted(
    ContentSettingsType permission) {
  RecordPermissionAction(permission, GRANTED);
}

void PermissionContextUmaUtil::PermissionDenied(
    ContentSettingsType permission) {
  RecordPermissionAction(permission, DENIED);
}

void PermissionContextUmaUtil::PermissionDismissed(
    ContentSettingsType permission) {
  RecordPermissionAction(permission, DISMISSED);
}

void PermissionContextUmaUtil::PermissionIgnored(
    ContentSettingsType permission) {
  RecordPermissionAction(permission, IGNORED);
}
