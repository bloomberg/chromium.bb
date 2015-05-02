// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_LOGGING_LOG_BUFFER_H
#define COMPONENTS_PROXIMITY_AUTH_LOGGING_LOG_BUFFER_H

#include <list>

#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace proximity_auth {

// Contains logs specific to the Proximity Auth. This buffer has a maximum size
// and will discard entries in FIFO order.
// Call LogBuffer::GetInstance() to get the global LogBuffer instance.
class LogBuffer {
 public:
  // Represents a single log entry in the log buffer.
  struct LogMessage {
    const std::string text;
    const base::Time time;
    const std::string file;
    const int line;
    const logging::LogSeverity severity;

    LogMessage(const std::string& text,
               const base::Time& time,
               const std::string& file,
               int line,
               logging::LogSeverity severity);
  };

  LogBuffer();
  ~LogBuffer();

  // Returns the global instance.
  static LogBuffer* GetInstance();

  // Adds a new log message to the buffer. If the number of log messages exceeds
  // the maximum, then the earliest added log will be removed.
  void AddLogMessage(const LogMessage& log_message);

  // Clears all logs in the buffer.
  void Clear();

  // Returns the maximum number of logs that can be stored.
  size_t MaxBufferSize() const;

  // Returns the list logs in the buffer, sorted chronologically.
  const std::list<LogMessage>* logs() { return &log_messages_; }

 private:
  // The messages currently in the buffer.
  std::list<LogMessage> log_messages_;

  DISALLOW_COPY_AND_ASSIGN(LogBuffer);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_LOGGING_LOG_BUFFER_H
