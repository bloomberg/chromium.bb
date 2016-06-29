// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/permission_reporter.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/permission_type.h"
#include "net/url_request/report_sender.h"

using content::PermissionType;

namespace safe_browsing {

namespace {
// URL to upload permission action reports.
const char kPermissionActionReportingUploadUrl[] =
    "http://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "permission-action";

PermissionReport::PermissionType PermissionTypeForReport(
    PermissionType permission) {
  switch (permission) {
    case PermissionType::MIDI_SYSEX:
      return PermissionReport::MIDI_SYSEX;
    case PermissionType::PUSH_MESSAGING:
      return PermissionReport::PUSH_MESSAGING;
    case PermissionType::NOTIFICATIONS:
      return PermissionReport::NOTIFICATIONS;
    case PermissionType::GEOLOCATION:
      return PermissionReport::GEOLOCATION;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return PermissionReport::PROTECTED_MEDIA_IDENTIFIER;
    case PermissionType::MIDI:
      return PermissionReport::MIDI;
    case PermissionType::DURABLE_STORAGE:
      return PermissionReport::DURABLE_STORAGE;
    case PermissionType::AUDIO_CAPTURE:
      return PermissionReport::AUDIO_CAPTURE;
    case PermissionType::VIDEO_CAPTURE:
      return PermissionReport::VIDEO_CAPTURE;
    case PermissionType::BACKGROUND_SYNC:
      return PermissionReport::UNKNOWN_PERMISSION;
    case PermissionType::NUM:
      break;
  }

  NOTREACHED();
  return PermissionReport::UNKNOWN_PERMISSION;
}

PermissionReport::Action PermissionActionForReport(PermissionAction action) {
  switch (action) {
    case GRANTED:
      return PermissionReport::GRANTED;
    case DENIED:
      return PermissionReport::DENIED;
    case DISMISSED:
      return PermissionReport::DISMISSED;
    case IGNORED:
      return PermissionReport::IGNORED;
    case REVOKED:
      return PermissionReport::REVOKED;
    case REENABLED:
    case REQUESTED:
      return PermissionReport::ACTION_UNSPECIFIED;
    case PERMISSION_ACTION_NUM:
      break;
  }

  NOTREACHED();
  return PermissionReport::ACTION_UNSPECIFIED;
}

}  // namespace

PermissionReporter::PermissionReporter(net::URLRequestContext* request_context)
    : PermissionReporter(base::WrapUnique(new net::ReportSender(
          request_context,
          net::ReportSender::CookiesPreference::DO_NOT_SEND_COOKIES))) {}

PermissionReporter::PermissionReporter(
    std::unique_ptr<net::ReportSender> report_sender)
    : permission_report_sender_(std::move(report_sender)) {}

PermissionReporter::~PermissionReporter() {}

void PermissionReporter::SendReport(const GURL& origin,
                                    content::PermissionType permission,
                                    PermissionAction action) {
  std::string serialized_report;
  BuildReport(origin, permission, action, &serialized_report);
  permission_report_sender_->Send(GURL(kPermissionActionReportingUploadUrl),
                                  serialized_report);
}

// static
bool PermissionReporter::BuildReport(const GURL& origin,
                                     PermissionType permission,
                                     PermissionAction action,
                                     std::string* output) {
  PermissionReport report;
  report.set_origin(origin.spec());
  report.set_permission(PermissionTypeForReport(permission));
  report.set_action(PermissionActionForReport(action));

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
  variations::GetFieldTrialActiveGroupIds(&active_group_ids);
  for (auto active_group_id : active_group_ids) {
    PermissionReport::FieldTrial* field_trial = report.add_field_trials();
    field_trial->set_name_id(active_group_id.name);
    field_trial->set_group_id(active_group_id.group);
  }
  return report.SerializeToString(output);
}

}  // namespace safe_browsing
