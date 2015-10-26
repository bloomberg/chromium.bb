// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context_uma_util.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/rappor/rappor_service.h"
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

using content::PermissionType;

namespace {

// Enum for UMA purposes, make sure you update histograms.xml if you add new
// permission actions. Never delete or reorder an entry; only add new entries
// immediately before PERMISSION_NUM
enum PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,
  REVOKED = 4,
  REENABLED = 5,
  REQUESTED = 6,

  // Always keep this at the end.
  PERMISSION_ACTION_NUM,
};

// Deprecated. This method is used for the single-dimensional RAPPOR metrics
// that are being replaced by the multi-dimensional ones.
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
    default:
      NOTREACHED();
      break;
  }

  std::string permission_str =
      PermissionUtil::GetPermissionString(permission);
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
      case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
        PERMISSION_ACTION_UMA(
            secure_origin, "ContentSettings.PermissionActions_DurableStorage",
            "ContentSettings.PermissionActionsSecureOrigin_DurableStorage",
            "ContentSettings.PermissionActionsInsecureOrigin_DurableStorage",
            action);
        break;
      default:
        NOTREACHED() << "PERMISSION " << permission << " not accounted for";
  }

  // There are two sets of semi-redundant RAPPOR metrics being reported:
  // The soon-to-be-deprecated single dimensional ones, and the new
  // multi-dimensional ones.
  rappor::RapporService* rappor_service = g_browser_process->rappor_service();
  const std::string rappor_metric = GetRapporMetric(permission, action);
  if (!rappor_metric.empty())
    rappor::SampleDomainAndRegistryFromGURL(
        rappor_service, rappor_metric, requesting_origin);

  // Add multi-dimensional RAPPOR reporting for safe-browsing users.
  std::string permission_str =
      PermissionUtil::GetPermissionString(permission);
  if (!rappor_service || permission_str.empty())
    return;

  scoped_ptr<rappor::Sample> sample =
      rappor_service->CreateSample(rappor::SAFEBROWSING_RAPPOR_TYPE);
  sample->SetStringField("Scheme", requesting_origin.scheme());
  sample->SetStringField("Host", requesting_origin.host());
  sample->SetStringField("Port", requesting_origin.port());
  sample->SetStringField("Domain",
      rappor::GetDomainAndRegistrySampleFromGURL(requesting_origin));
  sample->SetFlagsField("Actions",
                        1 << action,
                        PermissionAction::PERMISSION_ACTION_NUM);
  rappor_service->RecordSampleObj("Permissions.Action." +
      permission_str, sample.Pass());
}

std::string PermissionTypeToString(PermissionType permission_type) {
  switch (permission_type) {
    case PermissionType::MIDI_SYSEX:
      return "MidiSysex";
    case PermissionType::PUSH_MESSAGING:
      return "PushMessaging";
    case PermissionType::NOTIFICATIONS:
      return "Notifications";
    case PermissionType::GEOLOCATION:
      return "Geolocation";
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case PermissionType::DURABLE_STORAGE:
      return "DurableStorage";
    case PermissionType::MIDI:
      return "Midi";
    case PermissionType::AUDIO_CAPTURE:
      return "AudioRecording";
    case PermissionType::VIDEO_CAPTURE:
      return "VideoRecording";
    case PermissionType::NUM:
      break;
  }
  NOTREACHED();
  return std::string();
}

void RecordPermissionRequest(ContentSettingsType permission,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             Profile* profile) {
  bool secure_origin = content::IsOriginSecure(requesting_origin);
  PermissionType type;
  switch (permission) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      type = PermissionType::GEOLOCATION;
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.PermissionRequested.Geolocation.Url",
          requesting_origin);
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      type = PermissionType::NOTIFICATIONS;
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.PermissionRequested.Notifications.Url",
          requesting_origin);
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      type = PermissionType::MIDI_SYSEX;
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      type = PermissionType::PUSH_MESSAGING;
      break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
      break;
#endif
    case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
      type = content::PermissionType::DURABLE_STORAGE;
      break;
    default:
      NOTREACHED() << "PERMISSION " << permission << " not accounted for";
      return;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.PermissionRequested",
      static_cast<base::HistogramBase::Sample>(type),
      static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  if (secure_origin) {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_SecureOrigin",
        static_cast<base::HistogramBase::Sample>(type),
        static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_InsecureOrigin",
        static_cast<base::HistogramBase::Sample>(type),
        static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  }

  // In order to gauge the compatibility risk of implementing an improved
  // iframe permissions security model, we would like to know the ratio of
  // same-origin to cross-origin permission requests. Our estimate of this
  // ratio could be somewhat biased by repeated requests coming from a
  // single frame, but we expect this to be insignificant.
  if (requesting_origin.GetOrigin() != embedding_origin.GetOrigin()) {
    content::PermissionManager* manager = profile->GetPermissionManager();
    if (!manager)
      return;
    content::PermissionStatus embedding_permission_status =
        manager->GetPermissionStatus(type, embedding_origin, embedding_origin);

    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
        "Permissions.Requested.CrossOrigin_" + PermissionTypeToString(type), 1,
        content::PERMISSION_STATUS_LAST, content::PERMISSION_STATUS_LAST + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(embedding_permission_status);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Permissions.Requested.SameOrigin",
        static_cast<base::HistogramBase::Sample>(type),
        static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  }
}

} // namespace

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionContextUmaUtil::PermissionRequested(
    ContentSettingsType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    Profile* profile) {
  RecordPermissionRequest(permission, requesting_origin, embedding_origin,
                          profile);
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
