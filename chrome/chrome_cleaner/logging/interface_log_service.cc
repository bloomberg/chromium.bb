// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple class that takes care of logging all the calls to a given interface.
//
// This should run on the IPC thread.

#include "chrome/chrome_cleaner/logging/interface_log_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/logging/proto/interface_logger.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"

namespace chrome_cleaner {

namespace {

base::FilePath GetPathForLogFile(const base::string16& log_file_name) {
  base::FilePath app_data_path;
  GetAppDataProductDirectory(&app_data_path);
  base::FilePath log_file_path = app_data_path.Append(log_file_name);
  return log_file_path;
}

}  // namespace

base::TimeDelta InterfaceLogService::GetTicksSinceCreation() const {
  return base::TimeTicks::Now() - ticks_at_creation_;
}

LogInformation::LogInformation(std::string function_name, std::string file_name)
    : function_name(function_name), file_name(file_name) {}

// static
std::unique_ptr<InterfaceLogService> InterfaceLogService::Create(
    const base::string16& file_name) {
  if (file_name.empty())
    return nullptr;

  base::FilePath file_path = GetPathForLogFile(file_name);

  std::string file_path_utf8;
  if (!base::UTF16ToUTF8(file_path.value().c_str(), file_path.value().size(),
                         &file_path_utf8)) {
    LOG(ERROR) << "Can't open interface log file " << file_path.value()
               << ": name is invalid" << std::endl;
    return nullptr;
  }

  std::ofstream stream(file_path_utf8);
  if (!stream.is_open()) {
    PLOG(ERROR) << "Can't open interface log file " << file_path_utf8;
    return nullptr;
  }

  return base::WrapUnique<InterfaceLogService>(
      new InterfaceLogService(file_name, std::move(stream)));
}

InterfaceLogService::~InterfaceLogService() = default;

void InterfaceLogService::AddCall(
    const LogInformation& log_information,
    const std::map<std::string, std::string>& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (log_information.function_name.empty())
    return;

  int64_t microseconds_since_start = GetTicksSinceCreation().InMicroseconds();

  APICall* new_call = call_record_.add_api_calls();
  new_call->set_function_name(log_information.function_name);
  new_call->set_file_name(log_information.file_name);
  new_call->set_microseconds_since_start(microseconds_since_start);
  new_call->mutable_parameters()->insert(params.begin(), params.end());

  csv_stream_ << microseconds_since_start << "," << log_information.file_name
              << "," << log_information.function_name << ",";

  for (const auto& name_value : params)
    csv_stream_ << name_value.first << "=" << name_value.second << ";";

  csv_stream_ << std::endl;
}

void InterfaceLogService::AddCall(const LogInformation& log_information) {
  AddCall(log_information, {});
}

std::vector<APICall> InterfaceLogService::GetCallHistory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  std::vector<APICall> call_history;
  for (auto it = call_record_.api_calls().begin();
       it != call_record_.api_calls().end(); it++) {
    call_history.push_back(*it);
  }
  return call_history;
}

std::string InterfaceLogService::GetBuildVersion() const {
  return call_record_.build_version();
}

base::FilePath InterfaceLogService::GetLogFilePath() const {
  return GetPathForLogFile(log_file_name_);
}

InterfaceLogService::InterfaceLogService(const base::string16& file_name,
                                         std::ofstream csv_stream)
    : log_file_name_(file_name), csv_stream_(std::move(csv_stream)) {
  DETACH_FROM_SEQUENCE(sequence_check_);
  base::string16 build_version_utf16 = LASTCHANGE_STRING;
  std::string build_version_utf8;
  if (!base::UTF16ToUTF8(build_version_utf16.c_str(),
                         build_version_utf16.size(), &build_version_utf8)) {
    LOG(ERROR) << "Cannot convert build version to utf8";
    build_version_utf8 = "UNKNOWN";
  }
  call_record_.set_build_version(build_version_utf8);

  // Write build version and column headers.
  csv_stream_ << "buildVersion," << build_version_utf8 << std::endl;
  csv_stream_ << "timeCalledTicks,fileName,functionName,functionArguments"
              << std::endl;
}

}  // namespace chrome_cleaner
