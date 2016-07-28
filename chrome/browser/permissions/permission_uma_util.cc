// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_uma_util.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/origin_util.h"
#include "url/gurl.h"

// UMA keys need to be statically initialized so plain function would not
// work. Use macros instead.
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

#define PERMISSION_BUBBLE_TYPE_UMA(metric_name, permission_bubble_type) \
  UMA_HISTOGRAM_ENUMERATION(                                            \
      metric_name,                                                      \
      static_cast<base::HistogramBase::Sample>(permission_bubble_type), \
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::NUM))

#define PERMISSION_BUBBLE_GESTURE_TYPE_UMA(gesture_metric_name,              \
                                           no_gesture_metric_name,           \
                                           gesture_type,                     \
                                           permission_bubble_type)           \
  if (gesture_type == PermissionRequestGestureType::GESTURE) {               \
    PERMISSION_BUBBLE_TYPE_UMA(gesture_metric_name, permission_bubble_type); \
  } else if (gesture_type == PermissionRequestGestureType::NO_GESTURE) {     \
    PERMISSION_BUBBLE_TYPE_UMA(no_gesture_metric_name,                       \
                               permission_bubble_type);                      \
  }

using content::PermissionType;

namespace {

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

void RecordPermissionRequest(PermissionType permission,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             Profile* profile) {
  rappor::RapporService* rappor_service = g_browser_process->rappor_service();
  if (rappor_service) {
    if (permission == PermissionType::GEOLOCATION) {
      // TODO(dominickn): remove this deprecated metric - crbug.com/605836.
      rappor::SampleDomainAndRegistryFromGURL(
          rappor_service,
          "ContentSettings.PermissionRequested.Geolocation.Url",
          requesting_origin);
      rappor_service->RecordSample(
          "ContentSettings.PermissionRequested.Geolocation.Url2",
          rappor::LOW_FREQUENCY_ETLD_PLUS_ONE_RAPPOR_TYPE,
          rappor::GetDomainAndRegistrySampleFromGURL(requesting_origin));
    } else if (permission == PermissionType::NOTIFICATIONS) {
      // TODO(dominickn): remove this deprecated metric - crbug.com/605836.
      rappor::SampleDomainAndRegistryFromGURL(
          rappor_service,
          "ContentSettings.PermissionRequested.Notifications.Url",
          requesting_origin);
      rappor_service->RecordSample(
          "ContentSettings.PermissionRequested.Notifications.Url2",
          rappor::LOW_FREQUENCY_ETLD_PLUS_ONE_RAPPOR_TYPE,
          rappor::GetDomainAndRegistrySampleFromGURL(requesting_origin));
    } else if (permission == PermissionType::MIDI ||
               permission == PermissionType::MIDI_SYSEX) {
      // TODO(dominickn): remove this deprecated metric - crbug.com/605836.
      rappor::SampleDomainAndRegistryFromGURL(
          rappor_service,
          "ContentSettings.PermissionRequested.Midi.Url",
          requesting_origin);
      rappor_service->RecordSample(
          "ContentSettings.PermissionRequested.Midi.Url2",
          rappor::LOW_FREQUENCY_ETLD_PLUS_ONE_RAPPOR_TYPE,
          rappor::GetDomainAndRegistrySampleFromGURL(requesting_origin));
    }
  }

  bool secure_origin = content::IsOriginSecure(requesting_origin);
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

const char PermissionUmaUtil::kPermissionsPromptShown[] =
    "Permissions.Prompt.Shown";
const char PermissionUmaUtil::kPermissionsPromptShownGesture[] =
    "Permissions.Prompt.Shown.Gesture";
const char PermissionUmaUtil::kPermissionsPromptShownNoGesture[] =
    "Permissions.Prompt.Shown.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptAccepted[] =
    "Permissions.Prompt.Accepted";
const char PermissionUmaUtil::kPermissionsPromptAcceptedGesture[] =
    "Permissions.Prompt.Accepted.Gesture";
const char PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture[] =
    "Permissions.Prompt.Accepted.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptDenied[] =
    "Permissions.Prompt.Denied";
const char PermissionUmaUtil::kPermissionsPromptDeniedGesture[] =
    "Permissions.Prompt.Denied.Gesture";
const char PermissionUmaUtil::kPermissionsPromptDeniedNoGesture[] =
    "Permissions.Prompt.Denied.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptRequestsPerPrompt[] =
    "Permissions.Prompt.RequestsPerPrompt";
const char PermissionUmaUtil::kPermissionsPromptMergedBubbleTypes[] =
    "Permissions.Prompt.MergedBubbleTypes";
const char PermissionUmaUtil::kPermissionsPromptMergedBubbleAccepted[] =
    "Permissions.Prompt.MergedBubbleAccepted";
const char PermissionUmaUtil::kPermissionsPromptMergedBubbleDenied[] =
    "Permissions.Prompt.MergedBubbleDenied";

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionUmaUtil::PermissionRequested(PermissionType permission,
                                            const GURL& requesting_origin,
                                            const GURL& embedding_origin,
                                            Profile* profile) {
  RecordPermissionRequest(permission, requesting_origin, embedding_origin,
                          profile);
}

void PermissionUmaUtil::PermissionGranted(
    PermissionType permission,
    PermissionRequestGestureType gesture_type,
    const GURL& requesting_origin,
    Profile* profile) {
  RecordPermissionAction(permission, GRANTED, PermissionSourceUI::PROMPT,
                         gesture_type, requesting_origin, profile);
}

void PermissionUmaUtil::PermissionDenied(
    PermissionType permission,
    PermissionRequestGestureType gesture_type,
    const GURL& requesting_origin,
    Profile* profile) {
  RecordPermissionAction(permission, DENIED, PermissionSourceUI::PROMPT,
                         gesture_type, requesting_origin, profile);
}

void PermissionUmaUtil::PermissionDismissed(
    PermissionType permission,
    PermissionRequestGestureType gesture_type,
    const GURL& requesting_origin,
    Profile* profile) {
  RecordPermissionAction(permission, DISMISSED, PermissionSourceUI::PROMPT,
                         gesture_type, requesting_origin, profile);
}

void PermissionUmaUtil::PermissionIgnored(
    PermissionType permission,
    PermissionRequestGestureType gesture_type,
    const GURL& requesting_origin,
    Profile* profile) {
  RecordPermissionAction(permission, IGNORED, PermissionSourceUI::PROMPT,
                         gesture_type, requesting_origin, profile);
}

void PermissionUmaUtil::PermissionRevoked(PermissionType permission,
                                          PermissionSourceUI source_ui,
                                          const GURL& revoked_origin,
                                          Profile* profile) {
  // TODO(tsergeant): Expand metrics definitions for revocation to include all
  // permissions.
  if (permission == PermissionType::NOTIFICATIONS ||
      permission == PermissionType::GEOLOCATION ||
      permission == PermissionType::AUDIO_CAPTURE ||
      permission == PermissionType::VIDEO_CAPTURE) {
    // An unknown gesture type is passed in since gesture type is only
    // applicable in prompt UIs where revocations are not possible.
    RecordPermissionAction(permission, REVOKED, source_ui,
                           PermissionRequestGestureType::UNKNOWN,
                           revoked_origin, profile);
  }
}

void PermissionUmaUtil::PermissionPromptShown(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK(!requests.empty());

  PermissionRequestType permission_prompt_type =
      PermissionRequestType::MULTIPLE;
  PermissionRequestGestureType permission_gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    permission_prompt_type = requests[0]->GetPermissionRequestType();
    permission_gesture_type = requests[0]->GetGestureType();
  }

  PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptShown, permission_prompt_type);
  PERMISSION_BUBBLE_GESTURE_TYPE_UMA(
      kPermissionsPromptShownGesture, kPermissionsPromptShownNoGesture,
      permission_gesture_type, permission_prompt_type);

  UMA_HISTOGRAM_ENUMERATION(
      kPermissionsPromptRequestsPerPrompt,
      static_cast<base::HistogramBase::Sample>(requests.size()),
      static_cast<base::HistogramBase::Sample>(10));

  if (requests.size() > 1) {
    for (const auto* request : requests) {
      PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptMergedBubbleTypes,
                                 request->GetPermissionRequestType());
    }
  }
}

void PermissionUmaUtil::PermissionPromptAccepted(
    const std::vector<PermissionRequest*>& requests,
    const std::vector<bool>& accept_states) {
  DCHECK(!requests.empty());
  DCHECK(requests.size() == accept_states.size());

  bool all_accepted = accept_states[0];
  PermissionRequestType permission_prompt_type =
      requests[0]->GetPermissionRequestType();
  PermissionRequestGestureType permission_gesture_type =
      requests[0]->GetGestureType();
  if (requests.size() > 1) {
    permission_prompt_type = PermissionRequestType::MULTIPLE;
    permission_gesture_type = PermissionRequestGestureType::UNKNOWN;
    for (size_t i = 0; i < requests.size(); ++i) {
      const auto* request = requests[i];
      if (accept_states[i]) {
        PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptMergedBubbleAccepted,
                                   request->GetPermissionRequestType());
      } else {
        all_accepted = false;
        PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptMergedBubbleDenied,
                                   request->GetPermissionRequestType());
      }
    }
  }

  if (all_accepted) {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptAccepted,
                               permission_prompt_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(
        kPermissionsPromptAcceptedGesture, kPermissionsPromptAcceptedNoGesture,
        permission_gesture_type, permission_prompt_type);
  } else {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptDenied,
                               permission_prompt_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(
        kPermissionsPromptDeniedGesture, kPermissionsPromptDeniedNoGesture,
        permission_gesture_type, permission_prompt_type);
  }
}

void PermissionUmaUtil::PermissionPromptDenied(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK(!requests.empty());
  DCHECK(requests.size() == 1);

  PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptDenied,
                             requests[0]->GetPermissionRequestType());
  PERMISSION_BUBBLE_GESTURE_TYPE_UMA(
      kPermissionsPromptDeniedGesture, kPermissionsPromptDeniedNoGesture,
      requests[0]->GetGestureType(), requests[0]->GetPermissionRequestType());
}

bool PermissionUmaUtil::IsOptedIntoPermissionActionReporting(Profile* profile) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePermissionActionReporting))
    return false;

  DCHECK(profile);
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE)
    return false;
  if (!profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled))
    return false;

  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // Do not report if profile can't get a profile sync service due to disable
  // sync flag.
  if (!profile_sync_service)
    return false;

  if (!profile_sync_service->CanSyncStart())
    return false;

  syncer::ModelTypeSet preferred_data_types =
      profile_sync_service->GetPreferredDataTypes();
  if (!preferred_data_types.Has(syncer::PROXY_TABS))
    return false;
  if (!preferred_data_types.Has(syncer::PRIORITY_PREFERENCES))
    return false;

  return true;
}

void PermissionUmaUtil::RecordPermissionAction(
    PermissionType permission,
    PermissionAction action,
    PermissionSourceUI source_ui,
    PermissionRequestGestureType gesture_type,
    const GURL& requesting_origin,
    Profile* profile) {
  if (IsOptedIntoPermissionActionReporting(profile)) {
    g_browser_process->safe_browsing_service()
        ->ui_manager()
        ->ReportPermissionAction(requesting_origin, permission, action,
                                 source_ui, gesture_type);
  }

  bool secure_origin = content::IsOriginSecure(requesting_origin);

  switch (permission) {
    case PermissionType::GEOLOCATION:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.Geolocation",
                            "Permissions.Action.SecureOrigin.Geolocation",
                            "Permissions.Action.InsecureOrigin.Geolocation",
                            action);
      break;
    case PermissionType::NOTIFICATIONS:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.Notifications",
                            "Permissions.Action.SecureOrigin.Notifications",
                            "Permissions.Action.InsecureOrigin.Notifications",
                            action);
      break;
    case PermissionType::MIDI_SYSEX:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.MidiSysEx",
                            "Permissions.Action.SecureOrigin.MidiSysEx",
                            "Permissions.Action.InsecureOrigin.MidiSysEx",
                            action);
      break;
    case PermissionType::PUSH_MESSAGING:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.PushMessaging",
                            "Permissions.Action.SecureOrigin.PushMessaging",
                            "Permissions.Action.InsecureOrigin.PushMessaging",
                            action);
      break;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.ProtectedMedia",
                            "Permissions.Action.SecureOrigin.ProtectedMedia",
                            "Permissions.Action.InsecureOrigin.ProtectedMedia",
                            action);
      break;
    case PermissionType::DURABLE_STORAGE:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.DurableStorage",
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

  // Retrieve the name of the RAPPOR metric. Currently, the new metric name is
  // the deprecated name with "2" on the end, e.g.
  // ContentSettings.PermissionActions_Geolocation.Granted.Url2. For simplicity,
  // we retrieve the deprecated name and append the "2" for the new name.
  // TODO(dominickn): remove the deprecated metric and replace it solely with
  // the new one in GetRapporMetric - crbug.com/605836.
  const std::string deprecated_metric = GetRapporMetric(permission, action);
  rappor::RapporService* rappor_service = g_browser_process->rappor_service();
  if (!deprecated_metric.empty() && rappor_service) {
    rappor::SampleDomainAndRegistryFromGURL(rappor_service, deprecated_metric,
                                            requesting_origin);

    std::string rappor_metric = deprecated_metric + "2";
    rappor_service->RecordSample(
        rappor_metric, rappor::LOW_FREQUENCY_ETLD_PLUS_ONE_RAPPOR_TYPE,
        rappor::GetDomainAndRegistrySampleFromGURL(requesting_origin));
  }
}
