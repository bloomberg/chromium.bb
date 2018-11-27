// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <windows.h>

#include "base/logging.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"

namespace gwp_asan {
namespace internal {

crashpad::VMAddress CrashAnalyzer::GetAccessAddress(
    const crashpad::ExceptionSnapshot& exception) {
  if (exception.Exception() != EXCEPTION_ACCESS_VIOLATION)
    return 0;

  const std::vector<uint64_t>& codes = exception.Codes();
  if (codes.size() < 2) {
    DLOG(FATAL) << "Exception array is too small! " << codes.size();
    return 0;
  }

  return codes[1];
}

}  // namespace internal
}  // namespace gwp_asan
