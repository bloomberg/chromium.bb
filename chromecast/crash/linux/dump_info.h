// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_DUMP_INFO_H_
#define CHROMECAST_CRASH_LINUX_DUMP_INFO_H_

#include <string>

#include "base/macros.h"
#include "chromecast/crash/linux/minidump_params.h"

namespace chromecast {

// Class that encapsulates the construction and parsing of dump entries
// in the log file.
class DumpInfo {
 public:
  // Attempt to construct a DumpInfo object by parsing the given entry string
  // and extracting the contained information and populate the relevant
  // fields.
  explicit DumpInfo(const std::string& entry);

  // Attempt to construct a DumpInfo object that has the following info:
  //
  // -crashed_process_dump: the full path of the dump written
  // -crashed_process_logfile: the full path of the logfile written
  // -dump_time: the time of the dump written
  // -params: a structure containing other useful crash information
  //
  // As a result of construction, the |entry_| will be filled with the
  // appropriate string to add to the log file.
  DumpInfo(const std::string& crashed_process_dump,
           const std::string& crashed_process_logfile,
           const time_t& dump_time,
           const MinidumpParams& params);

  ~DumpInfo();

  const std::string& crashed_process_dump() const {
    return crashed_process_dump_;
  }
  const std::string& logfile() const { return logfile_; }
  const time_t& dump_time() const { return dump_time_; }
  const std::string& entry() const { return entry_; }
  const MinidumpParams& params() const { return params_; }
  const bool valid() const { return valid_; }

 private:
  bool ParseEntry(const std::string& entry);
  bool SetDumpTimeFromString(const std::string& timestr);
  std::string GetEntryAsString();

  std::string crashed_process_dump_;
  std::string logfile_;
  time_t dump_time_;
  std::string entry_;
  MinidumpParams params_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(DumpInfo);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_DUMP_INFO_H_
