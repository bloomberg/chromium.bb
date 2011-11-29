// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_LOGGING_H_
#define CHROME_BROWSER_SYNC_UTIL_LOGGING_H_
#pragma once

#include "base/logging.h"

// TODO(akalin): This probably belongs in base/ somewhere.

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace browser_sync {

bool VlogIsOnForLocation(const tracked_objects::Location& from_here,
                         int verbose_level);

}  // namespace browser_sync

#define VLOG_LOC_STREAM(from_here, verbose_level)                       \
  logging::LogMessage(from_here.file_name(), from_here.line_number(),   \
                      -verbose_level).stream()

#define DVLOG_LOC(from_here, verbose_level)                             \
  LAZY_STREAM(                                                          \
      VLOG_LOC_STREAM(from_here, verbose_level),                        \
      ::logging::DEBUG_MODE &&                                          \
      (VLOG_IS_ON(verbose_level) ||                                     \
       ::browser_sync::VlogIsOnForLocation(from_here, verbose_level)))  \

#endif  // CHROME_BROWSER_SYNC_UTIL_LOGGING_H_
