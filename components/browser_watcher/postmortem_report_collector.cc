// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/postmortem_report_collector.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browser_watcher/postmortem_minidump_writer.h"
#include "components/browser_watcher/stability_data_names.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

namespace browser_watcher {

using base::FilePath;
using crashpad::CrashReportDatabase;

namespace {

// DO NOT CHANGE VALUES. This is logged persistently in a histogram.
enum SystemSessionAnalysisStatus {
  SYSTEM_SESSION_ANALYSIS_SUCCESS = 0,
  SYSTEM_SESSION_ANALYSIS_NO_TIMESTAMP = 1,
  SYSTEM_SESSION_ANALYSIS_NO_ANALYZER = 2,
  SYSTEM_SESSION_ANALYSIS_FAILED = 3,
  SYSTEM_SESSION_ANALYSIS_OUTSIDE_RANGE = 4,
  SYSTEM_SESSION_ANALYSIS_STATUS_MAX = 5
};

bool GetStartTimestamp(
    const google::protobuf::Map<std::string, TypedValue>& global_data,
    base::Time* time) {
  DCHECK(time);

  const auto& it = global_data.find(kStabilityStartTimestamp);
  if (it == global_data.end())
    return false;

  const TypedValue& value = it->second;
  if (value.value_case() != TypedValue::kSignedValue)
    return false;

  *time = base::Time::FromInternalValue(value.signed_value());
  return true;
}

void LogCollectionStatus(CollectionStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ActivityTracker.Collect.Status", status,
                            COLLECTION_STATUS_MAX);
}

}  // namespace

void PostmortemDeleter::Process(
    const std::vector<base::FilePath>& stability_files) {
  for (const FilePath& file : stability_files) {
    if (base::DeleteFile(file, false))
      LogCollectionStatus(UNCLEAN_SHUTDOWN);
    else
      LogCollectionStatus(DEBUG_FILE_DELETION_FAILED);
  }
}

PostmortemReportCollector::PostmortemReportCollector(
    const std::string& product_name,
    const std::string& version_number,
    const std::string& channel_name,
    crashpad::CrashReportDatabase* report_database,
    SystemSessionAnalyzer* analyzer)
    : product_name_(product_name),
      version_number_(version_number),
      channel_name_(channel_name),
      report_database_(report_database),
      system_session_analyzer_(analyzer) {
  DCHECK_NE(nullptr, report_database);
}

PostmortemReportCollector::~PostmortemReportCollector() {}

void PostmortemReportCollector::Process(
    const std::vector<base::FilePath>& stability_files) {
  // Determine the crashpad client id.
  crashpad::UUID client_id;
  crashpad::Settings* settings = report_database_->GetSettings();
  if (settings) {
    // If GetSettings() or GetClientID() fails client_id will be left at its
    // default value, all zeroes, which is appropriate.
    settings->GetClientID(&client_id);
  }

  for (const FilePath& file : stability_files) {
    CollectAndSubmitOneReport(client_id, file);
  }
}

void PostmortemReportCollector::CollectAndSubmitOneReport(
    const crashpad::UUID& client_id,
    const FilePath& file) {
  DCHECK_NE(nullptr, report_database_);
  LogCollectionStatus(COLLECTION_ATTEMPT);

  // Note: the code below involves two notions of report: chrome internal state
  // reports and the crashpad reports they get wrapped into.

  // Collect the data from the debug file to a proto.
  StabilityReport report_proto;
  CollectionStatus status = CollectOneReport(file, &report_proto);
  if (status != SUCCESS) {
    // The file was empty, or there was an error collecting the data. Detailed
    // logging happens within the Collect function.
    if (!base::DeleteFile(file, false))
      DLOG(ERROR) << "Failed to delete " << file.value();
    LogCollectionStatus(status);
    return;
  }

  // Delete the stability file. If the file cannot be deleted, do not report its
  // contents - it will be retried in a future processing run. Note that this
  // approach can lead to under reporting and retries. However, under reporting
  // is preferable to the over reporting that would happen with a file that
  // cannot be deleted. Also note that the crash registration may still fail at
  // this point: losing the report in such a case is deemed acceptable.
  if (!base::DeleteFile(file, false)) {
    DLOG(ERROR) << "Failed to delete " << file.value();
    LogCollectionStatus(DEBUG_FILE_DELETION_FAILED);
    return;
  }

  LogCollectionStatus(UNCLEAN_SHUTDOWN);
  if (report_proto.system_state().session_state() == SystemState::UNCLEAN)
    LogCollectionStatus(UNCLEAN_SESSION);

  // Prepare a crashpad report.
  CrashReportDatabase::NewReport* new_report = nullptr;
  CrashReportDatabase::OperationStatus database_status =
      report_database_->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    LogCollectionStatus(PREPARE_NEW_CRASH_REPORT_FAILED);
    return;
  }
  CrashReportDatabase::CallErrorWritingCrashReport
      call_error_writing_crash_report(report_database_, new_report);

  // Write the report to a minidump.
  if (!WriteReportToMinidump(&report_proto, client_id, new_report->uuid,
                             reinterpret_cast<FILE*>(new_report->handle))) {
    LogCollectionStatus(WRITE_TO_MINIDUMP_FAILED);
    return;
  }

  // Finalize the report wrt the report database. Note that this doesn't trigger
  // an immediate upload, but Crashpad will eventually upload the report (as of
  // writing, the delay is on the order of up to 15 minutes).
  call_error_writing_crash_report.Disarm();
  crashpad::UUID unused_report_id;
  database_status = report_database_->FinishedWritingCrashReport(
      new_report, &unused_report_id);
  if (database_status != CrashReportDatabase::kNoError) {
    LogCollectionStatus(FINISHED_WRITING_CRASH_REPORT_FAILED);
    return;
  }

  LogCollectionStatus(SUCCESS);
}

CollectionStatus PostmortemReportCollector::CollectOneReport(
    const base::FilePath& file,
    StabilityReport* report) {
  DCHECK(report);

  CollectionStatus status = Extract(file, report);
  if (status != SUCCESS)
    return status;

  SetReporterDetails(report);
  RecordSystemShutdownState(report);

  return SUCCESS;
}

void PostmortemReportCollector::SetReporterDetails(
    StabilityReport* report) const {
  DCHECK(report);

  google::protobuf::Map<std::string, TypedValue>& global_data =
      *(report->mutable_global_data());

  // Reporter version details. These are useful as the reporter may be of a
  // different version.
  global_data[kStabilityReporterChannel].set_string_value(channel_name());
#if defined(ARCH_CPU_X86)
  global_data[kStabilityReporterPlatform].set_string_value(
      std::string("Win32"));
#elif defined(ARCH_CPU_X86_64)
  global_data[kStabilityReporterPlatform].set_string_value(
      std::string("Win64"));
#endif
  global_data[kStabilityReporterProduct].set_string_value(product_name());
  global_data[kStabilityReporterVersion].set_string_value(version_number());
}

void PostmortemReportCollector::RecordSystemShutdownState(
    StabilityReport* report) const {
  DCHECK(report);

  // The session state for the stability report, recorded to provided visibility
  // into whether the system session was clean.
  SystemState::SessionState session_state = SystemState::UNKNOWN;
  // The status of the analysis, recorded to provide insight into the success
  // or failure of the analysis.
  SystemSessionAnalysisStatus status = SYSTEM_SESSION_ANALYSIS_SUCCESS;

  base::Time time;
  if (!GetStartTimestamp(report->global_data(), &time)) {
    status = SYSTEM_SESSION_ANALYSIS_NO_TIMESTAMP;
  } else if (!system_session_analyzer_) {
    status = SYSTEM_SESSION_ANALYSIS_NO_ANALYZER;
  } else {
    SystemSessionAnalyzer::Status analyzer_status =
        system_session_analyzer_->IsSessionUnclean(time);
    switch (analyzer_status) {
      case SystemSessionAnalyzer::FAILED:
        status = SYSTEM_SESSION_ANALYSIS_FAILED;
        break;
      case SystemSessionAnalyzer::CLEAN:
        session_state = SystemState::CLEAN;
        break;
      case SystemSessionAnalyzer::UNCLEAN:
        session_state = SystemState::UNCLEAN;
        break;
      case SystemSessionAnalyzer::OUTSIDE_RANGE:
        status = SYSTEM_SESSION_ANALYSIS_OUTSIDE_RANGE;
        break;
    }
  }

  report->mutable_system_state()->set_session_state(session_state);
  UMA_HISTOGRAM_ENUMERATION(
      "ActivityTracker.Collect.SystemSessionAnalysisStatus", status,
      SYSTEM_SESSION_ANALYSIS_STATUS_MAX);
}

bool PostmortemReportCollector::WriteReportToMinidump(
    StabilityReport* report,
    const crashpad::UUID& client_id,
    const crashpad::UUID& report_id,
    base::PlatformFile minidump_file) {
  DCHECK(report);

  return WritePostmortemDump(minidump_file, client_id, report_id, report);
}

}  // namespace browser_watcher
