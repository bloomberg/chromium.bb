// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/logging/init_logging.h"

#include "base/command_line.h"
#include "base/logging.h"

namespace mojo {
namespace {

::logging::LoggingDestination DetermineLogMode(
    const base::CommandLine& command_line) {
  // only use OutputDebugString in debug mode
#if defined(NDEBUG)
  bool enable_logging = false;
  const char* kInvertLoggingSwitch = "enable-logging";
#else
  bool enable_logging = true;
  const char* kInvertLoggingSwitch = "disable-logging";
#endif
  const ::logging::LoggingDestination kDefaultLoggingMode =
      ::logging::LOG_TO_SYSTEM_DEBUG_LOG;

  if (command_line.HasSwitch(kInvertLoggingSwitch))
    enable_logging = !enable_logging;

  return enable_logging ? kDefaultLoggingMode : ::logging::LOG_NONE;
}

}  // namespace

void InitLogging() {
  ::logging::LoggingSettings settings;
  settings.logging_dest = DetermineLogMode(
      *base::CommandLine::ForCurrentProcess());
  ::logging::InitLogging(settings);

  // we want process and thread IDs because we have a lot of things running
  ::logging::SetLogItems(true,  // enable_process_id
                         true,  // enable_thread_id
                         true,  // enable_timestamp
                         false);  // enable_tickcount
}

}  // namespace mojo
