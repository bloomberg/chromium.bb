// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
#define COMPONENTS_NET_LOG_CHROME_NET_LOG_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "net/log/net_log.h"

namespace base {
class FilePath;
class Value;
}

namespace net {
class WriteToFileNetLogObserver;
class TraceNetLogObserver;
}

namespace net_log {

class NetLogFileWriter;

// ChromeNetLog is an implementation of NetLog that adds file loggers
// as its observers.
class ChromeNetLog : public net::NetLog {
 public:
  // The parameters to the constructor are only used for command-line based
  // NetLog writing (which starts immediately after construction).
  //
  // TODO(eroman): This would be clearer as a separate method to configure and
  //               start this logging mode.
  //
  // The log is saved to |log_file|.
  // |log_file_mode| is the mode used to log in |log_file|.
  // If |log_file| is empty, only a temporary log is created, and
  // |log_file_mode| is not used.
  ChromeNetLog(const base::FilePath& log_file,
               net::NetLogCaptureMode log_file_mode,
               const base::CommandLine::StringType& command_line_string,
               const std::string& channel_string);
  ~ChromeNetLog() override;

  // TODO(eroman): Rename this to something clearer. Perhaps
  //               |net_export_file_writer()|.
  NetLogFileWriter* net_log_file_writer();

  // Returns a Value containing constants needed to load a log file.
  // Safe to call on any thread.
  static std::unique_ptr<base::Value> GetConstants(
      const base::CommandLine::StringType& command_line_string,
      const std::string& channel_string);

 private:
  std::unique_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;
  std::unique_ptr<NetLogFileWriter> net_log_file_writer_;
  std::unique_ptr<net::TraceNetLogObserver> trace_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
