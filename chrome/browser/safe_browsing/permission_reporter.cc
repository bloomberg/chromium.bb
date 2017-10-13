// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/permission_reporter.h"

#include <functional>

#include "base/containers/queue.h"
#include "base/hash.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/time/default_clock.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "components/variations/active_field_trials.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"

namespace safe_browsing {

namespace {
// URL to upload permission action reports.
const char kPermissionActionReportingUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/"
    "chrome-permissions";

const int kMaximumReportsPerOriginPerPermissionPerMinute = 5;

PermissionReport::PermissionType PermissionTypeForReport(
    ContentSettingsType permission) {
  switch (permission) {
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return PermissionReport::MIDI_SYSEX;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return PermissionReport::PUSH_MESSAGING;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return PermissionReport::NOTIFICATIONS;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return PermissionReport::GEOLOCATION;
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return PermissionReport::PROTECTED_MEDIA_IDENTIFIER;
    case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
      return PermissionReport::DURABLE_STORAGE;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      return PermissionReport::AUDIO_CAPTURE;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      return PermissionReport::VIDEO_CAPTURE;
    case CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
      return PermissionReport::BACKGROUND_SYNC;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return PermissionReport::FLASH;
    default:
      break;
  }

  NOTREACHED();
  return PermissionReport::UNKNOWN_PERMISSION;
}

PermissionReport::Action PermissionActionForReport(PermissionAction action) {
  switch (action) {
    case PermissionAction::GRANTED:
      return PermissionReport::GRANTED;
    case PermissionAction::DENIED:
      return PermissionReport::DENIED;
    case PermissionAction::DISMISSED:
      return PermissionReport::DISMISSED;
    case PermissionAction::IGNORED:
      return PermissionReport::IGNORED;
    case PermissionAction::REVOKED:
      return PermissionReport::REVOKED;
    case PermissionAction::REENABLED:
    case PermissionAction::REQUESTED:
      return PermissionReport::ACTION_UNSPECIFIED;
    case PermissionAction::NUM:
      break;
  }

  NOTREACHED();
  return PermissionReport::ACTION_UNSPECIFIED;
}

PermissionReport::SourceUI SourceUIForReport(PermissionSourceUI source_ui) {
  switch (source_ui) {
    case PermissionSourceUI::PROMPT:
      return PermissionReport::PROMPT;
    case PermissionSourceUI::OIB:
      return PermissionReport::OIB;
    case PermissionSourceUI::SITE_SETTINGS:
      return PermissionReport::SITE_SETTINGS;
    case PermissionSourceUI::PAGE_ACTION:
      return PermissionReport::PAGE_ACTION;
    case PermissionSourceUI::NUM:
      break;
  }

  NOTREACHED();
  return PermissionReport::SOURCE_UI_UNSPECIFIED;
}

PermissionReport::GestureType GestureTypeForReport(
    PermissionRequestGestureType gesture_type) {
  switch (gesture_type) {
    case PermissionRequestGestureType::UNKNOWN:
      return PermissionReport::GESTURE_TYPE_UNSPECIFIED;
    case PermissionRequestGestureType::GESTURE:
      return PermissionReport::GESTURE;
    case PermissionRequestGestureType::NO_GESTURE:
      return PermissionReport::NO_GESTURE;
    case PermissionRequestGestureType::NUM:
      break;
  }

  NOTREACHED();
  return PermissionReport::GESTURE_TYPE_UNSPECIFIED;
}

PermissionReport::PersistDecision PersistDecisionForReport(
    PermissionPersistDecision persist_decision) {
  switch (persist_decision) {
    case PermissionPersistDecision::UNSPECIFIED:
      return PermissionReport::PERSIST_DECISION_UNSPECIFIED;
    case PermissionPersistDecision::PERSISTED:
      return PermissionReport::PERSISTED;
    case PermissionPersistDecision::NOT_PERSISTED:
      return PermissionReport::NOT_PERSISTED;
  }

  NOTREACHED();
  return PermissionReport::PERSIST_DECISION_UNSPECIFIED;
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("permission_reporting", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When a website prompts for a permission request, the origin and "
            "the action taken are reported to Google Safe Browsing for a "
            "subset of users. This is to find sites that are abusing "
            "permissions or asking much too often."
          trigger:
            "When a permission prompt closes for any reason, and the user is "
            "opted into safe browsing, metrics reporting, and history sync. "
            "These are throttled to less than five (permission, origin) pairs "
            "per minute."
          data:
            "The type of permission, the action taken on it, and the origin."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via either of the 'Protect you and "
            "your device from dangerous sites' setting under 'Privacy', or "
            "'Automatically send usage statistics and crash reports to Google' "
            "setting under 'Privacy' or 'History sync' setting under 'Sign in, "
            "Advanced sync settings'."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");

}  // namespace

bool PermissionAndOrigin::operator==(const PermissionAndOrigin& other) const {
  return (permission == other.permission && origin == other.origin);
}

std::size_t PermissionAndOriginHash::operator()(
    const PermissionAndOrigin& permission_and_origin) const {
  std::size_t permission_hash =
      static_cast<std::size_t>(permission_and_origin.permission);
  std::size_t origin_hash =
      std::hash<std::string>()(permission_and_origin.origin.spec());
  return base::HashInts(permission_hash, origin_hash);
}

PermissionReporter::PermissionReporter(net::URLRequestContext* request_context)
    : PermissionReporter(
          base::MakeUnique<net::ReportSender>(request_context,
                                              kTrafficAnnotation),
          base::WrapUnique(new base::DefaultClock)) {}

PermissionReporter::PermissionReporter(
    std::unique_ptr<net::ReportSender> report_sender,
    std::unique_ptr<base::Clock> clock)
    : permission_report_sender_(std::move(report_sender)),
      clock_(std::move(clock)) {}

PermissionReporter::~PermissionReporter() {}

void PermissionReporter::SendReport(const PermissionReportInfo& report_info) {
  if (IsReportThresholdExceeded(report_info.permission, report_info.origin))
    return;

  std::string serialized_report;
  BuildReport(report_info, &serialized_report);
  permission_report_sender_->Send(
      GURL(kPermissionActionReportingUploadUrl), "application/octet-stream",
      serialized_report, base::Callback<void()>(),
      base::Callback<void(const GURL&, int, int)>());
}

// static
bool PermissionReporter::BuildReport(const PermissionReportInfo& report_info,
                                     std::string* output) {
  PermissionReport report;
  report.set_origin(report_info.origin.spec());
  report.set_permission(PermissionTypeForReport(report_info.permission));
  report.set_action(PermissionActionForReport(report_info.action));
  report.set_source_ui(SourceUIForReport(report_info.source_ui));
  report.set_gesture(GestureTypeForReport(report_info.gesture_type));
  report.set_persisted(
      PersistDecisionForReport(report_info.persist_decision));
  report.set_num_prior_dismissals(report_info.num_prior_dismissals);
  report.set_num_prior_ignores(report_info.num_prior_ignores);

  // Collect platform data.
#if defined(OS_ANDROID)
  report.set_platform_type(PermissionReport::ANDROID_PLATFORM);
#elif defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS) || \
    defined(OS_LINUX)
  report.set_platform_type(PermissionReport::DESKTOP_PLATFORM);
#else
#error Unsupported platform.
#endif

  // Collect field trial data.
  std::vector<variations::ActiveGroupId> active_group_ids;
  variations::GetFieldTrialActiveGroupIds(base::StringPiece(),
                                          &active_group_ids);
  for (auto active_group_id : active_group_ids) {
    PermissionReport::FieldTrial* field_trial = report.add_field_trials();
    field_trial->set_name_id(active_group_id.name);
    field_trial->set_group_id(active_group_id.group);
  }
  return report.SerializeToString(output);
}

bool PermissionReporter::IsReportThresholdExceeded(
    ContentSettingsType permission,
    const GURL& origin) {
  base::queue<base::Time>& log = report_logs_[{permission, origin}];
  base::Time current_time = clock_->Now();
  // Remove entries that are sent more than one minute ago.
  while (!log.empty() &&
         current_time - log.front() > base::TimeDelta::FromMinutes(1)) {
    log.pop();
  }
  if (log.size() < kMaximumReportsPerOriginPerPermissionPerMinute) {
    log.push(current_time);
    return false;
  } else {
    return true;
  }
}

}  // namespace safe_browsing
