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

class NetExportFileWriter;

// ChromeNetLog is an implementation of NetLog that manages common observers
// (for --log-net-log, chrome://net-export/, tracing), as well as acting as the
// entry point for other consumers.
class ChromeNetLog : public net::NetLog {
 public:
  ChromeNetLog();
  ~ChromeNetLog() override;

  // Starts streaming the NetLog events to a file on disk. This will continue
  // until the application shuts down.
  // * |log_file| - path to write the file.
  // * |log_file_mode| - capture mode for event granularity.
  void StartWritingToFile(
      const base::FilePath& log_file,
      net::NetLogCaptureMode log_file_mode,
      const base::CommandLine::StringType& command_line_string,
      const std::string& channel_string);

  NetExportFileWriter* net_export_file_writer();

  // Returns a Value containing constants needed to load a log file.
  // Safe to call on any thread.
  static std::unique_ptr<base::Value> GetConstants(
      const base::CommandLine::StringType& command_line_string,
      const std::string& channel_string);

 private:
  std::unique_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;
  std::unique_ptr<NetExportFileWriter> net_export_file_writer_;
  std::unique_ptr<net::TraceNetLogObserver> trace_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
