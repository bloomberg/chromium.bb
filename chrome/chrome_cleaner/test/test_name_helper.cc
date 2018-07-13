// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_name_helper.h"
#include <ostream>

namespace chrome_cleaner {

std::ostream& operator<<(std::ostream& stream, ExecutionMode mode) {
  switch (mode) {
    case ExecutionMode::kNone:
      stream << "kNone";
      break;
    case ExecutionMode::kScanning:
      stream << "kScanning";
      break;
    case ExecutionMode::kCleanup:
      stream << "kCleanup";
      break;
    default:
      stream << mode;
  }
  return stream;
}

}  // namespace chrome_cleaner
