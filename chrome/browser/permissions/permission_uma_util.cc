// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_uma_util.h"

#include <utility>

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

// Deprecated. This method is used for the single-dimensional RAPPOR metrics
// that are being replaced by the multi-dimensional ones.
const std::string GetRapporMetric(PermissionType permission,
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
    case REVOKED:
      action_str = "Revoked";
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

void RecordPermissionAction(PermissionType permission,
                            PermissionAction action,
                            const GURL& requesting_origin) {
  bool secure_origin = content::IsOriginSecure(requesting_origin);

  switch (permission) {
    case PermissionType::GEOLOCATION:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "Permissions.Action.Geolocation",
            "Permissions.Action.SecureOrigin.Geolocation",
            "Permissions.Action.InsecureOrigin.Geolocation",
            action);
        break;
    case PermissionType::NOTIFICATIONS:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "Permissions.Action.Notifications",
            "Permissions.Action.SecureOrigin.Notifications",
            "Permissions.Action.InsecureOrigin.Notifications",
            action);
        break;
    case PermissionType::MIDI_SYSEX:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "Permissions.Action.MidiSysEx",
            "Permissions.Action.SecureOrigin.MidiSysEx",
            "Permissions.Action.InsecureOrigin.MidiSysEx",
            action);
        break;
    case PermissionType::PUSH_MESSAGING:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "Permissions.Action.PushMessaging",
            "Permissions.Action.SecureOrigin.PushMessaging",
            "Permissions.Action.InsecureOrigin.PushMessaging",
            action);
        break;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "Permissions.Action.ProtectedMedia",
            "Permissions.Action.SecureOrigin.ProtectedMedia",
            "Permissions.Action.InsecureOrigin.ProtectedMedia",
            action);
        break;
    case PermissionType::DURABLE_STORAGE:
        PERMISSION_ACTION_UMA(
            secure_origin,
            "Permissions.Action.DurableStorage",
            "Permissions.Action.SecureOrigin.DurableStorage",
            "Permissions.Action.InsecureOrigin.DurableStorage",
            action);
        break;
    case PermissionType::AUDIO_CAPTURE:
        // Media permissions are disabled on insecure origins, so there's no
        // need to record metrics for secure/insecue.
        UMA_HISTOGRAM_ENUMERATION("Permissions.Action.AudioCapture", action,
                                  PERMISSION_ACTION_NUM);
        break;
    case PermissionType::VIDEO_CAPTURE:
        UMA_HISTOGRAM_ENUMERATION("Permissions.Action.VideoCapture", action,
                                  PERMISSION_ACTION_NUM);
        break;
    // The user is not prompted for these permissions, thus there is no
    // permission action recorded for them.
    case PermissionType::MIDI:
    case PermissionType::BACKGROUND_SYNC:
    case PermissionType::NUM:
      NOTREACHED() << "PERMISSION "
                   << PermissionUtil::GetPermissionString(permission)
                   << " not accounted for";
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

  std::unique_ptr<rappor::Sample> sample =
      rappor_service->CreateSample(rappor::SAFEBROWSING_RAPPOR_TYPE);
  sample->SetStringField("Scheme", requesting_origin.scheme());
  sample->SetStringField("Host", requesting_origin.host());
  sample->SetStringField("Port", requesting_origin.port());
  sample->SetStringField("Domain",
      rappor::GetDomainAndRegistrySampleFromGURL(requesting_origin));
  sample->SetFlagsField("Actions", static_cast<uint64_t>(1) << action,
                        PermissionAction::PERMISSION_ACTION_NUM);
  rappor_service->RecordSampleObj("Permissions.Action." + permission_str,
                                  std::move(sample));
}

void RecordPermissionRequest(PermissionType permission,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             Profile* profile) {
  bool secure_origin = content::IsOriginSecure(requesting_origin);
  if (permission == PermissionType::GEOLOCATION) {
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.PermissionRequested.Geolocation.Url",
          requesting_origin);
  } else if (permission == PermissionType::NOTIFICATIONS) {
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(),
          "ContentSettings.PermissionRequested.Notifications.Url",
          requesting_origin);
  }
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.PermissionRequested",
      static_cast<base::HistogramBase::Sample>(permission),
      static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  if (secure_origin) {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_SecureOrigin",
        static_cast<base::HistogramBase::Sample>(permission),
        static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSettings.PermissionRequested_InsecureOrigin",
        static_cast<base::HistogramBase::Sample>(permission),
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
    blink::mojom::PermissionStatus embedding_permission_status =
        manager->GetPermissionStatus(permission, embedding_origin,
                                     embedding_origin);

    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
        "Permissions.Requested.CrossOrigin_" +
            PermissionUtil::GetPermissionString(permission),
        1, static_cast<int>(blink::mojom::PermissionStatus::LAST),
        static_cast<int>(blink::mojom::PermissionStatus::LAST) + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(static_cast<int>(embedding_permission_status));
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Permissions.Requested.SameOrigin",
        static_cast<base::HistogramBase::Sample>(permission),
        static_cast<base::HistogramBase::Sample>(PermissionType::NUM));
  }
}

}  // anonymous namespace

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionUmaUtil::PermissionRequested(PermissionType permission,
                                            const GURL& requesting_origin,
                                            const GURL& embedding_origin,
                                            Profile* profile) {
  RecordPermissionRequest(permission, requesting_origin, embedding_origin,
                          profile);
}

void PermissionUmaUtil::PermissionGranted(PermissionType permission,
                                          const GURL& requesting_origin) {
  RecordPermissionAction(permission, GRANTED, requesting_origin);
}

void PermissionUmaUtil::PermissionDenied(PermissionType permission,
                                         const GURL& requesting_origin) {
  RecordPermissionAction(permission, DENIED, requesting_origin);
}

void PermissionUmaUtil::PermissionDismissed(PermissionType permission,
                                            const GURL& requesting_origin) {
  RecordPermissionAction(permission, DISMISSED, requesting_origin);
}

void PermissionUmaUtil::PermissionIgnored(PermissionType permission,
                                          const GURL& requesting_origin) {
  RecordPermissionAction(permission, IGNORED, requesting_origin);
}

void PermissionUmaUtil::PermissionRevoked(PermissionType permission,
                                          const GURL& revoked_origin) {
  // TODO(tsergeant): Expand metrics definitions for revocation to include all
  // permissions.
  if (permission == PermissionType::NOTIFICATIONS ||
      permission == PermissionType::GEOLOCATION ||
      permission == PermissionType::AUDIO_CAPTURE ||
      permission == PermissionType::VIDEO_CAPTURE) {
    RecordPermissionAction(permission, REVOKED, revoked_origin);
  }
}
