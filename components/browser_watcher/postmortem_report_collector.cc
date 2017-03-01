// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/postmortem_report_collector.h"

#include <utility>

#include "base/debug/activity_analyzer.h"
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
#include "components/variations/active_field_trials.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

using base::FilePath;

namespace browser_watcher {

using ActivitySnapshot = base::debug::ThreadActivityAnalyzer::Snapshot;
using base::debug::ActivityUserData;
using base::debug::GlobalActivityAnalyzer;
using base::debug::GlobalActivityTracker;
using base::debug::ThreadActivityAnalyzer;
using crashpad::CrashReportDatabase;

namespace {

const char kFieldTrialKeyPrefix[] = "FieldTrial.";

// Collects stability user data from the recorded format to the collected
// format.
void CollectUserData(
    const ActivityUserData::Snapshot& recorded_map,
    google::protobuf::Map<std::string, TypedValue>* collected_map,
    StabilityReport* report) {
  DCHECK(collected_map);

  for (const auto& name_and_value : recorded_map) {
    const std::string& key = name_and_value.first;
    const ActivityUserData::TypedValue& recorded_value = name_and_value.second;
    TypedValue collected_value;

    switch (recorded_value.type()) {
      case ActivityUserData::END_OF_VALUES:
        NOTREACHED();
        break;
      case ActivityUserData::RAW_VALUE: {
        base::StringPiece raw = recorded_value.Get();
        collected_value.set_bytes_value(raw.data(), raw.size());
        break;
      }
      case ActivityUserData::RAW_VALUE_REFERENCE: {
        base::StringPiece recorded_ref = recorded_value.GetReference();
        TypedValue::Reference* collected_ref =
            collected_value.mutable_bytes_reference();
        collected_ref->set_address(
            reinterpret_cast<uintptr_t>(recorded_ref.data()));
        collected_ref->set_size(recorded_ref.size());
        break;
      }
      case ActivityUserData::STRING_VALUE: {
        base::StringPiece value = recorded_value.GetString();

        if (report && base::StartsWith(key, kFieldTrialKeyPrefix,
                                       base::CompareCase::SENSITIVE)) {
          // This entry represents an active Field Trial.
          std::string trial_name =
              key.substr(std::strlen(kFieldTrialKeyPrefix));
          variations::ActiveGroupId group_id =
              variations::MakeActiveGroupId(trial_name, value.as_string());
          FieldTrial* field_trial = report->add_field_trials();
          field_trial->set_name_id(group_id.name);
          field_trial->set_group_id(group_id.group);
          continue;
        }

        collected_value.set_string_value(value.data(), value.size());
        break;
      }
      case ActivityUserData::STRING_VALUE_REFERENCE: {
        base::StringPiece recorded_ref = recorded_value.GetStringReference();
        TypedValue::Reference* collected_ref =
            collected_value.mutable_string_reference();
        collected_ref->set_address(
            reinterpret_cast<uintptr_t>(recorded_ref.data()));
        collected_ref->set_size(recorded_ref.size());
        break;
      }
      case ActivityUserData::CHAR_VALUE: {
        char char_value = recorded_value.GetChar();
        collected_value.set_char_value(&char_value, 1);
        break;
      }
      case ActivityUserData::BOOL_VALUE:
        collected_value.set_bool_value(recorded_value.GetBool());
        break;
      case ActivityUserData::SIGNED_VALUE:
        collected_value.set_signed_value(recorded_value.GetInt());
        break;
      case ActivityUserData::UNSIGNED_VALUE:
        collected_value.set_unsigned_value(recorded_value.GetUint());
        break;
    }

    (*collected_map)[key].Swap(&collected_value);
  }
}

void CollectModuleInformation(
    const std::vector<GlobalActivityTracker::ModuleInfo>& modules,
    ProcessState* process_state) {
  DCHECK(process_state);

  char code_identifier[17];
  char debug_identifier[41];

  for (const GlobalActivityTracker::ModuleInfo& recorded : modules) {
    CodeModule* collected = process_state->add_modules();
    collected->set_base_address(recorded.address);
    collected->set_size(recorded.size);
    collected->set_code_file(recorded.file);

    // Compute the code identifier using the required format.
    snprintf(code_identifier, sizeof(code_identifier), "%08X%zx",
             recorded.timestamp, recorded.size);
    collected->set_code_identifier(code_identifier);
    collected->set_debug_file(recorded.debug_file);

    // Compute the debug identifier using the required format.
    const crashpad::UUID* uuid =
        reinterpret_cast<const crashpad::UUID*>(recorded.identifier);
    snprintf(debug_identifier, sizeof(debug_identifier),
             "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x", uuid->data_1,
             uuid->data_2, uuid->data_3, uuid->data_4[0], uuid->data_4[1],
             uuid->data_5[0], uuid->data_5[1], uuid->data_5[2], uuid->data_5[3],
             uuid->data_5[4], uuid->data_5[5], recorded.age);
    collected->set_debug_identifier(debug_identifier);
    collected->set_is_unloaded(!recorded.is_loaded);
  }
}

}  // namespace

PostmortemReportCollector::PostmortemReportCollector(
    const std::string& product_name,
    const std::string& version_number,
    const std::string& channel_name)
    : product_name_(product_name),
      version_number_(version_number),
      channel_name_(channel_name) {}

int PostmortemReportCollector::CollectAndSubmitForUpload(
    const base::FilePath& debug_info_dir,
    const base::FilePath::StringType& debug_file_pattern,
    const std::set<base::FilePath>& excluded_debug_files,
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
    const base::FilePath& debug_info_dir,
    const base::FilePath::StringType& debug_file_pattern,
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

PostmortemReportCollector::CollectionStatus
PostmortemReportCollector::CollectAndSubmit(
    const crashpad::UUID& client_id,
    const FilePath& file,
    crashpad::CrashReportDatabase* report_database) {
  DCHECK_NE(nullptr, report_database);

  // Note: the code below involves two notions of report: chrome internal state
  // reports and the crashpad reports they get wrapped into.

  // Collect the data from the debug file to a proto. Note: a non-empty report
  // is interpreted here as an unclean exit.
  std::unique_ptr<StabilityReport> report_proto;
  CollectionStatus status = Collect(file, &report_proto);
  if (status != SUCCESS) {
    // The file was empty, or there was an error collecting the data. Detailed
    // logging happens within the Collect function.
    if (!base::DeleteFile(file, false))
      DLOG(ERROR) << "Failed to delete " << file.value();
    return status;
  }
  DCHECK_NE(nullptr, report_proto.get());

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
  if (!WriteReportToMinidump(report_proto.get(), client_id, new_report->uuid,
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

PostmortemReportCollector::CollectionStatus PostmortemReportCollector::Collect(
    const base::FilePath& debug_state_file,
    std::unique_ptr<StabilityReport>* report) {
  DCHECK_NE(nullptr, report);
  report->reset();

  // Create a global analyzer.
  std::unique_ptr<GlobalActivityAnalyzer> global_analyzer =
      GlobalActivityAnalyzer::CreateWithFile(debug_state_file);
  if (!global_analyzer)
    return ANALYZER_CREATION_FAILED;

  // Early exit if there is no data.
  std::vector<std::string> log_messages = global_analyzer->GetLogMessages();
  ActivityUserData::Snapshot global_data_snapshot =
      global_analyzer->GetGlobalUserDataSnapshot();
  ThreadActivityAnalyzer* thread_analyzer = global_analyzer->GetFirstAnalyzer();
  if (log_messages.empty() && global_data_snapshot.empty() &&
      !thread_analyzer) {
    return DEBUG_FILE_NO_DATA;
  }

  // Create the report, then flesh it out.
  report->reset(new StabilityReport());

  // Collect log messages.
  for (const std::string& message : log_messages) {
    (*report)->add_log_messages(message);
  }

  // Collect global user data.
  google::protobuf::Map<std::string, TypedValue>& global_data =
      *(*report)->mutable_global_data();
  CollectUserData(global_data_snapshot, &global_data, report->get());

  // Add the reporting Chrome's details to the report.
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

  // Collect thread activity data.
  // Note: a single process is instrumented.
  ProcessState* process_state = (*report)->add_process_states();
  for (; thread_analyzer != nullptr;
       thread_analyzer = global_analyzer->GetNextAnalyzer()) {
    // Only valid analyzers are expected per contract of GetFirstAnalyzer /
    // GetNextAnalyzer.
    DCHECK(thread_analyzer->IsValid());

    if (!process_state->has_process_id()) {
      process_state->set_process_id(
          thread_analyzer->activity_snapshot().process_id);
    }
    DCHECK_EQ(thread_analyzer->activity_snapshot().process_id,
              process_state->process_id());

    ThreadState* thread_state = process_state->add_threads();
    CollectThread(thread_analyzer->activity_snapshot(), thread_state);
  }

  // Collect module information.
  CollectModuleInformation(global_analyzer->GetModules(), process_state);

  return SUCCESS;
}

void PostmortemReportCollector::CollectThread(
    const base::debug::ThreadActivityAnalyzer::Snapshot& snapshot,
    ThreadState* thread_state) {
  DCHECK(thread_state);

  thread_state->set_thread_name(snapshot.thread_name);
  thread_state->set_thread_id(snapshot.thread_id);
  thread_state->set_activity_count(snapshot.activity_stack_depth);

  for (size_t i = 0; i < snapshot.activity_stack.size(); ++i) {
    const base::debug::Activity& recorded = snapshot.activity_stack[i];
    Activity* collected = thread_state->add_activities();

    // Collect activity
    switch (recorded.activity_type) {
      case base::debug::Activity::ACT_TASK_RUN:
        collected->set_type(Activity::ACT_TASK_RUN);
        collected->set_origin_address(recorded.origin_address);
        collected->set_task_sequence_id(recorded.data.task.sequence_id);
        break;
      case base::debug::Activity::ACT_LOCK_ACQUIRE:
        collected->set_type(Activity::ACT_LOCK_ACQUIRE);
        collected->set_lock_address(recorded.data.lock.lock_address);
        break;
      case base::debug::Activity::ACT_EVENT_WAIT:
        collected->set_type(Activity::ACT_EVENT_WAIT);
        collected->set_event_address(recorded.data.event.event_address);
        break;
      case base::debug::Activity::ACT_THREAD_JOIN:
        collected->set_type(Activity::ACT_THREAD_JOIN);
        collected->set_thread_id(recorded.data.thread.thread_id);
        break;
      case base::debug::Activity::ACT_PROCESS_WAIT:
        collected->set_type(Activity::ACT_PROCESS_WAIT);
        collected->set_process_id(recorded.data.process.process_id);
        break;
      default:
        break;
    }

    // Collect user data
    if (i < snapshot.user_data_stack.size()) {
      CollectUserData(snapshot.user_data_stack[i],
                      collected->mutable_user_data(), nullptr);
    }
  }
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
