// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/environment/default_logger_impl.h"

#include "base/logging.h"
#include "base/macros.h"

namespace mojo {
namespace internal {
namespace {

int GetChromiumLogLevel(MojoLogLevel log_level) {
  // Our log levels correspond, except for "fatal":
  COMPILE_ASSERT(logging::LOG_VERBOSE == MOJO_LOG_LEVEL_VERBOSE,
                 verbose_log_level_value_mismatch);
  COMPILE_ASSERT(logging::LOG_INFO == MOJO_LOG_LEVEL_INFO,
                 info_log_level_value_mismatch);
  COMPILE_ASSERT(logging::LOG_WARNING == MOJO_LOG_LEVEL_WARNING,
                 warning_log_level_value_mismatch);
  COMPILE_ASSERT(logging::LOG_ERROR == MOJO_LOG_LEVEL_ERROR,
                 error_log_level_value_mismatch);

  return (log_level >= MOJO_LOG_LEVEL_FATAL) ? logging::LOG_FATAL : log_level;
}

void LogMessage(MojoLogLevel log_level, const char* message) {
  // TODO(vtl): Possibly, we should try to pull out the file and line number
  // from |message|.
  logging::LogMessage(__FILE__, __LINE__,
                      GetChromiumLogLevel(log_level)).stream() << message;
}

const MojoLogger kDefaultLogger = {
  LogMessage
};

}  // namespace

const MojoLogger* GetDefaultLoggerImpl() {
  return &kDefaultLogger;
}

}  // namespace internal
}  // namespace mojo
