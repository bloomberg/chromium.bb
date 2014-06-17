// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_ENVIRONMENT_LOGGER_H_
#define MOJO_PUBLIC_C_ENVIRONMENT_LOGGER_H_

#include <stdint.h>

// |MojoLogLevel|: Used to specify the type of log message. Values are ordered
// by severity (i.e., higher numerical values are more severe).

typedef int32_t MojoLogLevel;

#ifdef __cplusplus
const MojoLogLevel MOJO_LOG_LEVEL_VERBOSE = -1;
const MojoLogLevel MOJO_LOG_LEVEL_INFO = 0;
const MojoLogLevel MOJO_LOG_LEVEL_WARNING = 1;
const MojoLogLevel MOJO_LOG_LEVEL_ERROR = 2;
const MojoLogLevel MOJO_LOG_LEVEL_FATAL = 3;
#else
#define MOJO_LOG_LEVEL_VERBOSE ((MojoLogLevel) -1)
#define MOJO_LOG_LEVEL_INFO ((MojoLogLevel) 0)
#define MOJO_LOG_LEVEL_WARNING ((MojoLogLevel) 1)
#define MOJO_LOG_LEVEL_ERROR ((MojoLogLevel) 2)
#define MOJO_LOG_LEVEL_FATAL ((MojoLogLevel) 3)
#endif

// Structure with basic logging functions (on top of which more friendly logging
// macros may be built). The functions are thread-safe.
struct MojoLogger {
  // Logs |message| at level |log_level| if |log_level| is at least the current
  // minimum log level. If |log_level| is |MOJO_LOG_LEVEL_FATAL| (or greater),
  // aborts the application/process.
  void (*LogMessage)(MojoLogLevel log_level, const char* message);

  // Gets the minimum log level (see above), which will always be at most
  // |MOJO_LOG_LEVEL_FATAL|. (Though |LogMessage()| will automatically avoid
  // logging messages below the minimum log level, this may be used to avoid
  // extra work.)
  MojoLogLevel (*GetMinimumLogLevel)(void);
};

#endif  // MOJO_PUBLIC_C_ENVIRONMENT_LOGGER_H_
