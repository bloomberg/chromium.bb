// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/postmortem_report_collector.h"

#include <utility>

#include "base/files/file_enumerator.h"
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

PostmortemReportCollector::PostmortemReportCollector(
    const std::string& product_name,
    const std::string& version_number,
    const std::string& channel_name)
    : product_name_(product_name),
      version_number_(version_number),
      channel_name_(channel_name) {}

PostmortemReportCollector::~PostmortemReportCollector() {}

int PostmortemReportCollector::CollectAndSubmitForUpload(
    const FilePath& debug_info_dir,
    const FilePath::StringType& debug_file_pattern,
    const std::set<FilePath>& excluded_debug_files,
    crashpad::CrashReportDatabase* report_database) {
  DCHECK_NE(true, debug_info_dir.empty());
  DCHECK_NE(true, debug_file_pattern.empty());
  DCHECK_NE(nullptr, report_database);

  // Collect the list of files to harvest.
  std::vector<FilePath> debug_files = GetDebugStateFilePaths(
      debug_info_dir, debug_file_pattern, excluded_debug_files);
  UMA_HISTOGRAM_COUNTS_100("ActivityTracker.Collect.StabilityFileCount",
                           debug_files.size());

  // Determine the crashpad client id.
  crashpad::UUID client_id;
  crashpad::Settings* settings = report_database->GetSettings();
  if (settings) {
    // If GetSettings() or GetClientID() fails client_id will be left at its
    // default value, all zeroes, which is appropriate.
    settings->GetClientID(&client_id);
  }

  // Process each stability file.
  int success_cnt = 0;
  for (const FilePath& file : debug_files) {
    CollectionStatus status =
        CollectAndSubmit(client_id, file, report_database);
    // TODO(manzagop): consider making this a stability metric.
    UMA_HISTOGRAM_ENUMERATION("ActivityTracker.Collect.Status", status,
                              COLLECTION_STATUS_MAX);
    if (status == SUCCESS)
      ++success_cnt;
  }

  return success_cnt;
}

std::vector<FilePath> PostmortemReportCollector::GetDebugStateFilePaths(
    const FilePath& debug_info_dir,
    const FilePath::StringType& debug_file_pattern,
    const std::set<FilePath>& excluded_debug_files) {
  DCHECK_NE(true, debug_info_dir.empty());
  DCHECK_NE(true, debug_file_pattern.empty());

  std::vector<FilePath> paths;
  base::FileEnumerator enumerator(debug_info_dir, false /* recursive */,
                                  base::FileEnumerator::FILES,
                                  debug_file_pattern);
  FilePath path;
  for (path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    if (excluded_debug_files.find(path) == excluded_debug_files.end())
      paths.push_back(path);
  }
  return paths;
}

CollectionStatus PostmortemReportCollector::CollectAndSubmit(
    const crashpad::UUID& client_id,
    const FilePath& file,
    crashpad::CrashReportDatabase* report_database) {
  DCHECK_NE(nullptr, report_database);

  // Note: the code below involves two notions of report: chrome internal state
  // reports and the crashpad reports they get wrapped into.

  // Collect the data from the debug file to a proto. Note: a non-empty report
  // is interpreted here as an unclean exit.
  StabilityReport report_proto;
  CollectionStatus status = Collect(file, &report_proto);
  if (status != SUCCESS) {
    // The file was empty, or there was an error collecting the data. Detailed
    // logging happens within the Collect function.
    if (!base::DeleteFile(file, false))
      DLOG(ERROR) << "Failed to delete " << file.value();
    return status;
  }

  // Prepare a crashpad report.
  CrashReportDatabase::NewReport* new_report = nullptr;
  CrashReportDatabase::OperationStatus database_status =
      report_database->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    // Assume this is recoverable: not deleting the file.
    DLOG(ERROR) << "PrepareNewCrashReport failed";
    return PREPARE_NEW_CRASH_REPORT_FAILED;
  }
  CrashReportDatabase::CallErrorWritingCrashReport
      call_error_writing_crash_report(report_database, new_report);

  // Write the report to a minidump.
  if (!WriteReportToMinidump(&report_proto, client_id, new_report->uuid,
                             reinterpret_cast<FILE*>(new_report->handle))) {
    // Assume this is not recoverable and delete the file.
    if (!base::DeleteFile(file, false))
      DLOG(ERROR) << "Failed to delete " << file.value();
    return WRITE_TO_MINIDUMP_FAILED;
  }

  // If the file cannot be deleted, do not report its contents. Note this can
  // lead to under reporting and retries. However, under reporting is
  // preferable to the over reporting that would happen with a file that
  // cannot be deleted.
  // TODO(manzagop): metrics for the number of non-deletable files.
  if (!base::DeleteFile(file, false)) {
    DLOG(ERROR) << "Failed to delete " << file.value();
    return DEBUG_FILE_DELETION_FAILED;
  }

  // Finalize the report wrt the report database. Note that this doesn't trigger
  // an immediate upload, but Crashpad will eventually upload the report (as of
  // writing, the delay is on the order of up to 15 minutes).
  call_error_writing_crash_report.Disarm();
  crashpad::UUID unused_report_id;
  database_status = report_database->FinishedWritingCrashReport(
      new_report, &unused_report_id);
  if (database_status != CrashReportDatabase::kNoError) {
    DLOG(ERROR) << "FinishedWritingCrashReport failed";
    return FINISHED_WRITING_CRASH_REPORT_FAILED;
  }

  return SUCCESS;
}

CollectionStatus PostmortemReportCollector::Collect(const base::FilePath& file,
                                                    StabilityReport* report) {
  DCHECK(report);
  CollectionStatus status = Extract(file, report);
  if (status != SUCCESS)
    return status;

  // Add the reporter's details to the report.
  google::protobuf::Map<std::string, TypedValue>& global_data =
      *(report->mutable_global_data());
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

  return SUCCESS;
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
