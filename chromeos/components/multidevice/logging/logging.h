// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_LOGGING_LOGGING_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_LOGGING_LOGGING_H_

#include <sstream>

#include "base/logging.h"
#include "base/macros.h"

namespace chromeos {

namespace multidevice {

// Use the PA_LOG() macro for all logging related to Proximity Auth, so the
// system is aware of all logs related to this feature. We display these logs in
// the debug WebUI (chrome://proximity-auth).
//
// PA_LOG() has the same interface as the standard LOG() macro and also creates
// a normal log message of the same severity.
// Examples:
//   PA_LOG(INFO) << "Waiting for " << x << " pending requests.";
//   PA_LOG(ERROR) << "Request failed: " << error_string;
#define PA_LOG(severity)                                           \
  chromeos::multidevice::ScopedLogMessage(__FILE__, __LINE__,      \
                                          logging::LOG_##severity) \
      .stream()

// Disables all logging while in scope. Intended to be called only from test
// code, to clean up test output.
class ScopedDisableLoggingForTesting {
 public:
  ScopedDisableLoggingForTesting();
  ~ScopedDisableLoggingForTesting();
};

// An intermediate object used by the PA_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system and also added to Proximity Auth
// specific log buffer.  You should use the PA_LOG() macro instead of this class
// directly.
class ScopedLogMessage {
 public:
  ScopedLogMessage(const char* file, int line, logging::LogSeverity severity);
  ~ScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogMessage);
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_LOGGING_LOGGING_H_
