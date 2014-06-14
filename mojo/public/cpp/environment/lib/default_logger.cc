// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/environment/default_logger.h"

#include <stdio.h>
#include <stdlib.h>  // For |abort()|.

namespace mojo {
namespace {

const char* GetLogLevelString(MojoLogLevel log_level) {
  if (log_level < MOJO_LOG_LEVEL_VERBOSE)
    return "VERBOSE";
  switch (log_level) {
    case MOJO_LOG_LEVEL_VERBOSE:
      return "VERBOSE";
    case MOJO_LOG_LEVEL_INFO:
      return "INFO";
    case MOJO_LOG_LEVEL_WARNING:
      return "WARNING";
    case MOJO_LOG_LEVEL_ERROR:
      return "ERROR";
  }
  // Consider everything higher to be fatal.
  return "FATAL";
}

void LogMessage(MojoLogLevel log_level, const char* message) {
  // TODO(vtl): Add timestamp also?
  fprintf(stderr, "%s:%s\n", GetLogLevelString(log_level), message);
  if (log_level >= MOJO_LOG_LEVEL_FATAL)
    abort();
}

const MojoLogger kDefaultLogger = {
  LogMessage
};

}  // namespace

const MojoLogger* GetDefaultLogger() {
  return &kDefaultLogger;
}

}  // namespace mojo
