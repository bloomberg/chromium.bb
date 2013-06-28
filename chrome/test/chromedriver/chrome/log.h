// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LOG_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LOG_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/time/time.h"

// Abstract class for logging entries with a level, timestamp, string message.
class Log {
 public:
  // Log entry severity level.
  enum Level {
    kDebug,
    kLog,
    kWarning,
    kError
  };

  virtual ~Log() {}

  // Adds an entry to the log.
  virtual void AddEntryTimestamped(const base::Time& timestamp,
                                   Level level,
                                   const std::string& message) = 0;

  // Adds an entry to the log, timestamped with the current time.
  void AddEntry(Level level, const std::string& message);
};

// Writes log entries using printf.
class Logger : public Log {
 public:
  // Creates a logger with a minimum level of |kDebug|.
  Logger();
  explicit Logger(Level min_log_level);
  virtual ~Logger();

  virtual void AddEntryTimestamped(const base::Time& timestamp,
                                   Level level,
                                   const std::string& message) OVERRIDE;

 private:
  Level min_log_level_;
  base::Time start_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LOG_H_
