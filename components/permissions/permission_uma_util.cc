// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "components/permissions/android/jni_headers/PermissionUmaUtil_jni.h"
#endif

namespace permissions {

// UMA keys need to be statically initialized so plain function would not
// work. Use macros instead.
#define PERMISSION_ACTION_UMA(secure_origin, permission, permission_secure, \
                              permission_insecure, action)                  \
  base::UmaHistogramEnumeration(permission, action, PermissionAction::NUM); \
  if (secure_origin) {                                                      \
    base::UmaHistogramEnumeration(permission_secure, action,                \
                                  PermissionAction::NUM);                   \
  } else {                                                                  \
    base::UmaHistogramEnumeration(permission_insecure, action,              \
                                  PermissionAction::NUM);                   \
  }

#define PERMISSION_BUBBLE_TYPE_UMA(metric_name, permission_bubble_type) \
  base::UmaHistogramEnumeration(metric_name, permission_bubble_type,    \
                                PermissionRequestType::NUM)

#define PERMISSION_BUBBLE_GESTURE_TYPE_UMA(                                  \
    gesture_metric_name, no_gesture_metric_name, gesture_type,               \
    permission_bubble_type)                                                  \
  if (gesture_type == PermissionRequestGestureType::GESTURE) {               \
    PERMISSION_BUBBLE_TYPE_UMA(gesture_metric_name, permission_bubble_type); \
  } else if (gesture_type == PermissionRequestGestureType::NO_GESTURE) {     \
    PERMISSION_BUBBLE_TYPE_UMA(no_gesture_metric_name,                       \
                               permission_bubble_type);                      \
  }

using content::PermissionType;

namespace {

const int kPriorCountCap = 10;

std::string GetPermissionRequestString(PermissionRequestType type) {
  switch (type) {
    case PermissionRequestType::MULTIPLE:
      return "AudioAndVideoCapture";
    case PermissionRequestType::QUOTA:
      return "Quota";
    case PermissionRequestType::DOWNLOAD:
      return "MultipleDownload";
    case PermissionRequestType::REGISTER_PROTOCOL_HANDLER:
      return "RegisterProtocolHandler";
    case PermissionRequestType::PERMISSION_GEOLOCATION:
      return "Geolocation";
    case PermissionRequestType::PERMISSION_MIDI_SYSEX:
      return "MidiSysEx";
    case PermissionRequestType::PERMISSION_NOTIFICATIONS:
      return "Notifications";
    case PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMedia";
    case PermissionRequestType::PERMISSION_FLASH:
      return "Flash";
    case PermissionRequestType::PERMISSION_MEDIASTREAM_MIC:
      return "AudioCapture";
    case PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case PermissionRequestType::PERMISSION_SECURITY_KEY_ATTESTATION:
      return "SecurityKeyAttestation";
    case PermissionRequestType::PERMISSION_PAYMENT_HANDLER:
      return "PaymentHandler";
    case PermissionRequestType::PERMISSION_NFC:
      return "Nfc";
    case PermissionRequestType::PERMISSION_CLIPBOARD_READ_WRITE:
      return "ClipboardReadWrite";
    case PermissionRequestType::PERMISSION_VR:
      return "VR";
    case PermissionRequestType::PERMISSION_AR:
      return "AR";
    case PermissionRequestType::PERMISSION_STORAGE_ACCESS:
      return "StorageAccess";
    case PermissionRequestType::PERMISSION_CAMERA_PAN_TILT_ZOOM:
      return "CameraPanTiltZoom";
    case PermissionRequestType::PERMISSION_WINDOW_PLACEMENT:
      return "WindowPlacement";
    default:
      NOTREACHED();
      return "";
  }
}

void RecordEngagementMetric(const std::vector<PermissionRequest*>& requests,
                            content::WebContents* web_contents,
                            const std::string& action) {
  PermissionRequestType type = requests[0]->GetPermissionRequestType();
  if (requests.size() > 1)
    type = PermissionRequestType::MULTIPLE;

  DCHECK(action == "Accepted" || action == "Denied" || action == "Dismissed" ||
         action == "Ignored");
  std::string name = "Permissions.Engagement." + action + '.' +
                     GetPermissionRequestString(type);

  double engagement_score = PermissionsClient::Get()->GetSiteEngagementScore(
      web_contents->GetBrowserContext(), requests[0]->GetOrigin());
  base::UmaHistogramPercentage(name, engagement_score);
}

void RecordPermissionActionUkm(PermissionAction action,
                               PermissionRequestGestureType gesture_type,
                               ContentSettingsType permission,
                               int dismiss_count,
                               int ignore_count,
                               PermissionSourceUI source_ui,
                               PermissionPromptDisposition ui_disposition,
                               base::Optional<ukm::SourceId> source_id) {
  // Only record the permission change if the origin is in the history.
  if (!source_id.has_value())
    return;

  size_t num_values = 0;
  ukm::builders::Permission(source_id.value())
      .SetAction(static_cast<int64_t>(action))
      .SetGesture(static_cast<int64_t>(gesture_type))
      .SetPermissionType(static_cast<int64_t>(
          ContentSettingTypeToHistogramValue(permission, &num_values)))
      .SetPriorDismissals(std::min(kPriorCountCap, dismiss_count))
      .SetPriorIgnores(std::min(kPriorCountCap, ignore_count))
      .SetSource(static_cast<int64_t>(source_ui))
      .SetPromptDisposition(static_cast<int64_t>(ui_disposition))
      .Record(ukm::UkmRecorder::Get());
}

std::string GetPromptDispositionString(
    PermissionPromptDisposition ui_disposition) {
  switch (ui_disposition) {
    case PermissionPromptDisposition::ANCHORED_BUBBLE:
      return "AnchoredBubble";
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON:
      return "LocationBarRightAnimatedIcon";
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON:
      return "LocationBarRightStaticIcon";
    case PermissionPromptDisposition::MINI_INFOBAR:
      return "MiniInfobar";
    case PermissionPromptDisposition::MODAL_DIALOG:
      return "ModalDialog";
    case PermissionPromptDisposition::NOT_APPLICABLE:
      return "NotApplicable";
  }

  NOTREACHED();
  return "";
}

}  // anonymous namespace

// PermissionUmaUtil ----------------------------------------------------------

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

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionUmaUtil::PermissionRequested(ContentSettingsType content_type,
                                            const GURL& requesting_origin) {
  PermissionType permission;
  bool success = PermissionUtil::GetPermissionType(content_type, &permission);
  DCHECK(success);

  bool secure_origin = content::IsOriginSecure(requesting_origin);
  base::UmaHistogramEnumeration("ContentSettings.PermissionRequested",
                                permission, PermissionType::NUM);
  if (secure_origin) {
    base::UmaHistogramEnumeration(
        "ContentSettings.PermissionRequested_SecureOrigin", permission,
        PermissionType::NUM);
  } else {
    base::UmaHistogramEnumeration(
        "ContentSettings.PermissionRequested_InsecureOrigin", permission,
        PermissionType::NUM);
  }
}

void PermissionUmaUtil::PermissionRevoked(
    ContentSettingsType permission,
    PermissionSourceUI source_ui,
    const GURL& revoked_origin,
    content::BrowserContext* browser_context) {
  // TODO(tsergeant): Expand metrics definitions for revocation to include all
  // permissions.
  if (permission == ContentSettingsType::NOTIFICATIONS ||
      permission == ContentSettingsType::GEOLOCATION ||
      permission == ContentSettingsType::MEDIASTREAM_MIC ||
      permission == ContentSettingsType::MEDIASTREAM_CAMERA) {
    // An unknown gesture type is passed in since gesture type is only
    // applicable in prompt UIs where revocations are not possible.
    RecordPermissionAction(permission, PermissionAction::REVOKED, source_ui,
                           PermissionRequestGestureType::UNKNOWN,
                           PermissionPromptDisposition::NOT_APPLICABLE,
                           revoked_origin,
                           /*web_contents=*/nullptr, browser_context);
  }
}

void PermissionUmaUtil::RecordEmbargoPromptSuppression(
    PermissionEmbargoStatus embargo_status) {
  base::UmaHistogramEnumeration(
      "Permissions.AutoBlocker.EmbargoPromptSuppression", embargo_status,
      PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(
    PermissionStatusSource source) {
  // Explicitly switch to ensure that any new
  // PermissionStatusSource values are dealt with appropriately.
  switch (source) {
    case PermissionStatusSource::MULTIPLE_DISMISSALS:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_DISMISSALS);
      break;
    case PermissionStatusSource::MULTIPLE_IGNORES:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_IGNORES);
      break;
    case PermissionStatusSource::UNSPECIFIED:
    case PermissionStatusSource::KILL_SWITCH:
    case PermissionStatusSource::INSECURE_ORIGIN:
    case PermissionStatusSource::FEATURE_POLICY:
    case PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
      // The permission wasn't under embargo, so don't record anything. We may
      // embargo it later.
      break;
  }
}

void PermissionUmaUtil::RecordEmbargoStatus(
    PermissionEmbargoStatus embargo_status) {
  base::UmaHistogramEnumeration("Permissions.AutoBlocker.EmbargoStatus",
                                embargo_status, PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::PermissionPromptShown(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK(!requests.empty());

  PermissionRequestType request_type = PermissionRequestType::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = requests[0]->GetPermissionRequestType();
    gesture_type = requests[0]->GetGestureType();
  }

  PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptShown, request_type);
  PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptShownGesture,
                                     kPermissionsPromptShownNoGesture,
                                     gesture_type, request_type);
}

void PermissionUmaUtil::PermissionPromptResolved(
    const std::vector<PermissionRequest*>& requests,
    content::WebContents* web_contents,
    PermissionAction permission_action,
    PermissionPromptDisposition ui_disposition) {
  std::string action_string;

  switch (permission_action) {
    case PermissionAction::GRANTED:
      RecordPromptDecided(requests, /*accepted=*/true);
      action_string = "Accepted";
      break;
    case PermissionAction::DENIED:
      RecordPromptDecided(requests, /*accepted=*/false);
      action_string = "Denied";
      break;
    case PermissionAction::DISMISSED:
      action_string = "Dismissed";
      break;
    case PermissionAction::IGNORED:
      action_string = "Ignored";
      break;
    default:
      NOTREACHED();
      break;
  }
  RecordEngagementMetric(requests, web_contents, action_string);

  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          web_contents->GetBrowserContext());

  for (PermissionRequest* request : requests) {
    ContentSettingsType permission = request->GetContentSettingsType();
    // TODO(timloh): We only record these metrics for permissions which use
    // PermissionRequestImpl as the other subclasses don't support
    // GetGestureType and GetContentSettingsType.
    if (permission == ContentSettingsType::DEFAULT)
      continue;

    PermissionRequestGestureType gesture_type = request->GetGestureType();
    const GURL& requesting_origin = request->GetOrigin();

    RecordPermissionAction(permission, permission_action,
                           PermissionSourceUI::PROMPT, gesture_type,
                           ui_disposition, requesting_origin, web_contents,
                           web_contents->GetBrowserContext());

    std::string priorDismissPrefix =
        "Permissions.Prompt." + action_string + ".PriorDismissCount2.";
    std::string priorIgnorePrefix =
        "Permissions.Prompt." + action_string + ".PriorIgnoreCount2.";
    RecordPermissionPromptPriorCount(
        permission, priorDismissPrefix,
        autoblocker->GetDismissCount(requesting_origin, permission));
    RecordPermissionPromptPriorCount(
        permission, priorIgnorePrefix,
        autoblocker->GetIgnoreCount(requesting_origin, permission));
#if defined(OS_ANDROID)
    if (permission == ContentSettingsType::GEOLOCATION &&
        permission_action != PermissionAction::IGNORED) {
      RecordWithBatteryBucket("Permissions.BatteryLevel." + action_string +
                              ".Geolocation");
    }
#endif
  }

  base::UmaHistogramEnumeration("Permissions.Action.WithDisposition." +
                                    GetPromptDispositionString(ui_disposition),
                                permission_action, PermissionAction::NUM);
}

void PermissionUmaUtil::RecordPermissionPromptPriorCount(
    ContentSettingsType permission,
    const std::string& prefix,
    int count) {
  // The user is not prompted for this permissions, thus there is no prompt
  // event to record a prior count for.
  DCHECK_NE(ContentSettingsType::BACKGROUND_SYNC, permission);

  // Expand UMA_HISTOGRAM_COUNTS_100 so that we can use a dynamically suffixed
  // histogram name.
  base::Histogram::FactoryGet(
      prefix + PermissionUtil::GetPermissionString(permission), 1, 100, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(count);
}

#if defined(OS_ANDROID)
void PermissionUmaUtil::RecordWithBatteryBucket(const std::string& histogram) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionUmaUtil_recordWithBatteryBucket(
      env, base::android::ConvertUTF8ToJavaString(env, histogram));
}
#endif

void PermissionUmaUtil::RecordInfobarDetailsExpanded(bool expanded) {
  base::UmaHistogramBoolean("Permissions.Prompt.Infobar.DetailsExpanded",
                            expanded);
}

void PermissionUmaUtil::RecordMissingPermissionInfobarShouldShow(
    bool should_show,
    const std::vector<ContentSettingsType>& content_settings_types) {
  for (const auto& content_settings_type : content_settings_types) {
    base::UmaHistogramBoolean(
        "Permissions.MissingOSLevelPermission.ShouldShow." +
            PermissionUtil::GetPermissionString(content_settings_type),
        should_show);
  }
}

void PermissionUmaUtil::RecordMissingPermissionInfobarAction(
    PermissionAction action,
    const std::vector<ContentSettingsType>& content_settings_types) {
  for (const auto& content_settings_type : content_settings_types) {
    base::UmaHistogramEnumeration(
        "Permissions.MissingOSLevelPermission.Action." +
            PermissionUtil::GetPermissionString(content_settings_type),
        action, PermissionAction::NUM);
  }
}

PermissionUmaUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    content::BrowserContext* browser_context,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : browser_context_(browser_context),
      primary_url_(primary_url),
      secondary_url_(secondary_url),
      content_type_(content_type),
      source_ui_(source_ui) {
  if (!primary_url_.is_valid() ||
      (!secondary_url_.is_valid() && !secondary_url_.is_empty())) {
    is_initially_allowed_ = false;
    return;
  }
  HostContentSettingsMap* settings_map =
      PermissionsClient::Get()->GetSettingsMap(browser_context_);
  ContentSetting initial_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_, std::string());
  is_initially_allowed_ = initial_content_setting == CONTENT_SETTING_ALLOW;
}

PermissionUmaUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    content::BrowserContext* browser_context,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : ScopedRevocationReporter(
          browser_context,
          GURL(primary_pattern.ToString()),
          GURL((secondary_pattern == ContentSettingsPattern::Wildcard())
                   ? primary_pattern.ToString()
                   : secondary_pattern.ToString()),
          content_type,
          source_ui) {}

PermissionUmaUtil::ScopedRevocationReporter::~ScopedRevocationReporter() {
  if (!is_initially_allowed_)
    return;
  HostContentSettingsMap* settings_map =
      PermissionsClient::Get()->GetSettingsMap(browser_context_);
  ContentSetting final_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_, std::string());
  if (final_content_setting != CONTENT_SETTING_ALLOW) {
    // PermissionUmaUtil takes origins, even though they're typed as GURL.
    GURL requesting_origin = primary_url_.GetOrigin();
    PermissionRevoked(content_type_, source_ui_, requesting_origin,
                      browser_context_);
  }
}

void PermissionUmaUtil::RecordPermissionAction(
    ContentSettingsType permission,
    PermissionAction action,
    PermissionSourceUI source_ui,
    PermissionRequestGestureType gesture_type,
    PermissionPromptDisposition ui_disposition,
    const GURL& requesting_origin,
    const content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context);
  int dismiss_count =
      autoblocker->GetDismissCount(requesting_origin, permission);
  int ignore_count = autoblocker->GetIgnoreCount(requesting_origin, permission);

  PermissionsClient::Get()->GetUkmSourceId(
      browser_context, web_contents, requesting_origin,
      base::BindOnce(&RecordPermissionActionUkm, action, gesture_type,
                     permission, dismiss_count, ignore_count, source_ui,
                     ui_disposition));

  bool secure_origin = content::IsOriginSecure(requesting_origin);

  switch (permission) {
    // Geolocation, MidiSysEx, Push, Media, Clipboard, and AR/VR permissions are
    // disabled on insecure origins, so there's no need to record separate
    // metrics for secure/insecure.
    case ContentSettingsType::GEOLOCATION:
      base::UmaHistogramEnumeration("Permissions.Action.Geolocation", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::NOTIFICATIONS:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.Notifications",
                            "Permissions.Action.SecureOrigin.Notifications",
                            "Permissions.Action.InsecureOrigin.Notifications",
                            action);
      break;
    case ContentSettingsType::MIDI_SYSEX:
      base::UmaHistogramEnumeration("Permissions.Action.MidiSysEx", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.ProtectedMedia",
                            "Permissions.Action.SecureOrigin.ProtectedMedia",
                            "Permissions.Action.InsecureOrigin.ProtectedMedia",
                            action);
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      base::UmaHistogramEnumeration("Permissions.Action.AudioCapture", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      base::UmaHistogramEnumeration("Permissions.Action.VideoCapture", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::PLUGINS:
      PERMISSION_ACTION_UMA(secure_origin, "Permissions.Action.Flash",
                            "Permissions.Action.SecureOrigin.Flash",
                            "Permissions.Action.InsecureOrigin.Flash", action);
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      base::UmaHistogramEnumeration("Permissions.Action.ClipboardReadWrite",
                                    action, PermissionAction::NUM);
      break;
    case ContentSettingsType::PAYMENT_HANDLER:
      base::UmaHistogramEnumeration("Permissions.Action.PaymentHandler", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::NFC:
      base::UmaHistogramEnumeration("Permissions.Action.Nfc", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::VR:
      base::UmaHistogramEnumeration("Permissions.Action.VR", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::AR:
      base::UmaHistogramEnumeration("Permissions.Action.AR", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      base::UmaHistogramEnumeration("Permissions.Action.StorageAccess", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      base::UmaHistogramEnumeration("Permissions.Action.CameraPanTiltZoom",
                                    action, PermissionAction::NUM);
      break;
    case ContentSettingsType::WINDOW_PLACEMENT:
      base::UmaHistogramEnumeration("Permissions.Action.WindowPlacement",
                                    action, PermissionAction::NUM);
      break;
    // The user is not prompted for these permissions, thus there is no
    // permission action recorded for them.
    default:
      NOTREACHED() << "PERMISSION "
                   << PermissionUtil::GetPermissionString(permission)
                   << " not accounted for";
  }
}

// static
void PermissionUmaUtil::RecordPromptDecided(
    const std::vector<PermissionRequest*>& requests,
    bool accepted) {
  DCHECK(!requests.empty());

  PermissionRequestType request_type = PermissionRequestType::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = requests[0]->GetPermissionRequestType();
    gesture_type = requests[0]->GetGestureType();
  }

  if (accepted) {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptAccepted, request_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptAcceptedGesture,
                                       kPermissionsPromptAcceptedNoGesture,
                                       gesture_type, request_type);
  } else {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptDenied, request_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptDeniedGesture,
                                       kPermissionsPromptDeniedNoGesture,
                                       gesture_type, request_type);
  }
}

}  // namespace permissions
