// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/logging.h"

#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "chrome/browser/google_apis/event_logger.h"

namespace drive {
namespace util {
namespace {

static base::LazyInstance<google_apis::EventLogger> g_logger =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void Log(const char* format, ...) {
  std::string what;

  va_list args;
  va_start(args, format);
  base::StringAppendV(&what, format, args);
  va_end(args);

  // On thread-safety: LazyInstance guarantees thread-safety for the object
  // creation. EventLogger::Log() internally maintains the lock.
  google_apis::EventLogger* ptr = g_logger.Pointer();
  ptr->Log("%s", what.c_str());
}

std::vector<google_apis::EventLogger::Event> GetLogHistory() {
  google_apis::EventLogger* ptr = g_logger.Pointer();
  return ptr->GetHistory();
}

}  // namespace util
}  // namespace drive
