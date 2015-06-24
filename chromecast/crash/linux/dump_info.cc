// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/crash/linux/dump_info.h"

#include <stdlib.h>

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace chromecast {

namespace {

const char kDumpTimeFormat[] = "%Y-%m-%d %H:%M:%S";
const unsigned kDumpTimeMaxLen = 255;

const int kNumRequiredParams = 5;
}  // namespace

DumpInfo::DumpInfo(const std::string& entry) : valid_(false) {
  // TODO(slan): This ctor is doing non-trivial work. Change this.
  if ((valid_ = ParseEntry(entry)) == true) {
    entry_ = GetEntryAsString();
  }
}

DumpInfo::DumpInfo(const std::string& crashed_process_dump,
                   const std::string& logfile,
                   const time_t& dump_time,
                   const MinidumpParams& params)
    : crashed_process_dump_(crashed_process_dump),
      logfile_(logfile),
      dump_time_(dump_time),
      params_(params),
      valid_(false) {
  // The format is
  // <name>|<dump time>|<dump>|<uptime>|<logfile>|<suffix>|<prev_app_name>|
  // <curent_app name>|<last_app_name>
  // <dump time> is in the format of kDumpTimeFormat

  // Validate the time passed in.
  struct tm* tm = gmtime(&dump_time);
  char buf[kDumpTimeMaxLen];
  int n = strftime(buf, kDumpTimeMaxLen, kDumpTimeFormat, tm);
  if (n <= 0) {
    LOG(INFO) << "strftime failed";
    return;
  }
  entry_ = GetEntryAsString();
  valid_ = true;
}

DumpInfo::~DumpInfo() {
}

std::string DumpInfo::GetEntryAsString() {
  struct tm* tm = gmtime(&dump_time_);
  char buf[kDumpTimeMaxLen];
  int n = strftime(buf, kDumpTimeMaxLen, kDumpTimeFormat, tm);
  DCHECK_GT(n, 0);

  std::stringstream entrystream;
  entrystream << params_.process_name << "|" << buf << "|"
              << crashed_process_dump_ << "|" << params_.process_uptime << "|"
              << logfile_ << "|" << params_.suffix << "|"
              << params_.previous_app_name << "|" << params_.current_app_name
              << "|" << params_.last_app_name << "|"
              << params_.cast_release_version << "|"
              << params_.cast_build_number << std::endl;
  return entrystream.str();
}

bool DumpInfo::ParseEntry(const std::string& entry) {
  // The format is
  // <name>|<dump time>|<dump>|<uptime>|<logfile>{|<suffix>{|<prev_app_name>{
  //    |<current_app name>{|last_launched_app_name}}}}
  // <dump time> is in the format |kDumpTimeFormat|
  std::vector<std::string> fields;
  base::SplitString(entry, '|', &fields);
  if (fields.size() < kNumRequiredParams) {
    LOG(INFO) << "Invalid entry: Too few fields.";
    return false;
  }

  // Extract required fields.
  params_.process_name = fields[0];
  if (!SetDumpTimeFromString(fields[1]))
    return false;
  crashed_process_dump_ = fields[2];
  params_.process_uptime = atoll(fields[3].c_str());
  logfile_ = fields[4];

  // Extract all other optional fields.
  for (size_t i = 5; i < fields.size(); ++i) {
    const std::string& temp = fields[i];
    switch (i) {
      case 5:  // Optional field: suffix
        params_.suffix = temp;
        break;
      case 6:  // Optional field: prev_app_name
        params_.previous_app_name = temp;
        break;
      case 7:  // Optional field: current_app_name
        params_.current_app_name = temp;
        break;
      case 8:  // Optional field: last_launched_app_name
        params_.last_app_name = temp;
        break;
      case 9:  // extract an optional cast release version
        params_.cast_release_version = temp;
        break;
      case 10:  // extract an optional cast build number
        params_.cast_build_number = temp;
        break;
      default:
        LOG(INFO) << "Entry has too many fields invalid";
        return false;
    }
  }
  valid_ = true;
  return true;
}

bool DumpInfo::SetDumpTimeFromString(const std::string& timestr) {
  struct tm tm = {0};
  char* text = strptime(timestr.c_str(), kDumpTimeFormat, &tm);
  dump_time_ = mktime(&tm);
  if (!text || dump_time_ < 0) {
    LOG(INFO) << "Failed to convert dump time invalid";
    return false;
  }
  return true;
}

}  // namespace chromecast
