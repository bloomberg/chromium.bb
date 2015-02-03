// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_CRASH_SERVICE_CAPS_LOGGER_WIN_H_
#define CHROME_TOOLS_CRASH_SERVICE_CAPS_LOGGER_WIN_H_

#include <windows.h>

#include "base/macros.h"

namespace base {
class FilePath;
}

namespace caps {
// Creates a human-readable activity log file.
class Logger {
public:
  explicit Logger(const base::FilePath& path);
  ~Logger();

private:
  HANDLE file_;

  DISALLOW_COPY_AND_ASSIGN(Logger);
};

}  // namespace caps

#endif  // CHROME_TOOLS_CRASH_SERVICE_CAPS_LOGGER_WIN_H_

