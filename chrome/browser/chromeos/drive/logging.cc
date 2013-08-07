// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/logging.h"

#include <stdarg.h>   // va_list
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/event_logger.h"

namespace drive {
namespace util {
namespace {

static base::LazyInstance<EventLogger> g_logger =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void Log(logging::LogSeverity severity, const char* format, ...) {
  std::string what;

  va_list args;
  va_start(args, format);
  base::StringAppendV(&what, format, args);
  va_end(args);

  DVLOG(1) << what;

  // On thread-safety: LazyInstance guarantees thread-safety for the object
  // creation. EventLogger::Log() internally maintains the lock.
  EventLogger* ptr = g_logger.Pointer();
  ptr->Log(severity, what);
}

std::vector<EventLogger::Event> GetLogHistory() {
  EventLogger* ptr = g_logger.Pointer();
  return ptr->GetHistory();
}

}  // namespace util
}  // namespace drive
