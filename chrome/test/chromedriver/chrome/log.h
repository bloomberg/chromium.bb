// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LOG_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LOG_H_

#include <string>

namespace base {
class Time;
}

// Accepts log entries that have a level, timestamp, and a string message.
class Log {
 public:
  enum Level {
    kDebug,
    kLog,
    kWarning,
    kError
  };

  virtual ~Log() {}

  // Log a message with an explicit timestamp.
  virtual void AddEntry(const base::Time& time,
                        Level level,
                        const std::string& message) = 0;

  // Implicit timestamp, default to current time.
  void AddEntry(Level level, const std::string& message);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LOG_H_
