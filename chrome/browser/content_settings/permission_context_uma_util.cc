// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/permission_context_uma_util.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/origin_util.h"
#include "url/gurl.h"

// UMA keys need to be statically initialized so plain function would not
// work. Use a Macro instead.
#define PERMISSION_ACTION_UMA(secure_origin, permission, permission_secure, \
                              permission_insecure, action)                  \
  UMA_HISTOGRAM_ENUMERATION(permission, action, PERMISSION_ACTION_NUM);     \
  if (secure_origin) {                                                      \
    UMA_HISTOGRAM_ENUMERATION(permission_secure, action,                    \
                              PERMISSION_ACTION_NUM);                       \
  } else {                                                                  \
    UMA_HISTOGRAM_ENUMERATION(permission_insecure, action,                  \
                              PERMISSION_ACTION_NUM);                       \
  }

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

const std::string GetRapporMetric(ContentSettingsType permission,
                                  PermissionAction action) {
  std::string action_str;
  switch (action) {
    case GRANTED:
      action_str = "Granted";
      break;
    case DENIED:
      action_str = "Denied";
      break;
    case DISMISSED:
      action_str = "Dismissed";
      break;
    case IGNORED:
      action_str = "Ignored";
      break;
    case PERMISSION_ACTION_NUM:
      NOTREACHED();
      break;
  }
  std::string permission_str;
  switch (permission) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      permission_str = "Geolocation";
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      permission_str = "Notifications";
      break;
    default:
      permission_str = "";
      break;
  }

  if (permission_str.empty())
    return "";
  return base::StringPrintf("ContentSettings.PermissionActions_%s.%s.Url",
                            permission_str.c_str(), action_str.c_str());
}

void RecordPermissionAction(ContentSettingsType permission,
                            PermissionAction action,
                            const GURL& requesting_origin) {
  bool secure_origin = content::IsOriginSecure(requesting_origin);

  switch (permission) {
      case CONTENT_SETTINGS_TYPE_GEOLOCATION:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "ContentSettings.PermissionActions_Geolocation",
            "ContentSettings.PermissionActionsSecureOrigin_Geolocation",
            "ContentSettings.PermissionActionsInsecureOrigin_Geolocation",
            action);
        break;
      case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "ContentSettings.PermissionActions_Notifications",
            "ContentSettings.PermissionActionsSecureOrigin_Notifications",
            "ContentSettings.PermissionActionsInsecureOrigin_Notifications",
            action);
        break;
      case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "ContentSettings.PermissionActions_MidiSysEx",
            "ContentSettings.PermissionActionsSecureOrigin_MidiSysEx",
            "ContentSettings.PermissionActionsInsecureOrigin_MidiSysEx",
            action);
        break;
      case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "ContentSettings.PermissionActions_PushMessaging",
            "ContentSettings.PermissionActionsSecureOrigin_PushMessaging",
            "ContentSettings.PermissionActionsInsecureOrigin_PushMessaging",
            action);
        break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
      case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "ContentSettings.PermissionActions_ProtectedMedia",
            "ContentSettings.PermissionActionsSecureOrigin_ProtectedMedia",
            "ContentSettings.PermissionActionsInsecureOrigin_ProtectedMedia",
            action);
        break;
#endif
      default:
        NOTREACHED() << "PERMISSION " << permission << " not accounted for";
    }

    const std::string& rappor_metric = GetRapporMetric(permission, action);
    if (!rappor_metric.empty())
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(), rappor_metric,
          requesting_origin);
}

void RecordPermissionRequest(ContentSettingsType permission,
                             const GURL& requesting_origin) {
  bool secure_origin = content::IsOriginSecure(requesting_origin);
  content::PermissionType type;
  switch (permission) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      type = content::PermissionType::GEOLOCATION;
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.PermissionRequested.Geolocation.Url",
          requesting_origin);
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      type = content::PermissionType::NOTIFICATIONS;
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.PermissionRequested.Notifications.Url",
          requesting_origin);
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      type = content::PermissionType::MIDI_SYSEX;
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      type = content::PermissionType::PUSH_MESSAGING;
      break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      type = content::PermissionType::PROTECTED_MEDIA_IDENTIFIER;
      break;
#endif
    default:
      NOTREACHED() << "PERMISSION " << permission << " not accounted for";
      return;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.PermissionRequested",
      static_cast<base::HistogramBase::Sample>(type),
      static_cast<base::HistogramBase::Sample>(content::PermissionType::NUM));
  if (secure_origin) {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_SecureOrigin",
        static_cast<base::HistogramBase::Sample>(type),
        static_cast<base::HistogramBase::Sample>(content::PermissionType::NUM));
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_InsecureOrigin",
        static_cast<base::HistogramBase::Sample>(type),
        static_cast<base::HistogramBase::Sample>(content::PermissionType::NUM));
  }
}

} // namespace

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionContextUmaUtil::PermissionRequested(
    ContentSettingsType permission, const GURL& requesting_origin) {
  RecordPermissionRequest(permission, requesting_origin);
}

void PermissionContextUmaUtil::PermissionGranted(
    ContentSettingsType permission, const GURL& requesting_origin) {
  RecordPermissionAction(permission, GRANTED, requesting_origin);
}

void PermissionContextUmaUtil::PermissionDenied(
    ContentSettingsType permission, const GURL& requesting_origin) {
  RecordPermissionAction(permission, DENIED, requesting_origin);
}

void PermissionContextUmaUtil::PermissionDismissed(
    ContentSettingsType permission, const GURL& requesting_origin) {
  RecordPermissionAction(permission, DISMISSED, requesting_origin);
}

void PermissionContextUmaUtil::PermissionIgnored(
    ContentSettingsType permission, const GURL& requesting_origin) {
  RecordPermissionAction(permission, IGNORED, requesting_origin);
}
