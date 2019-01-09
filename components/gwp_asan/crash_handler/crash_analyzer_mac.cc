// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <mach/exception_types.h>

#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"

namespace gwp_asan {
namespace internal {

crashpad::VMAddress CrashAnalyzer::GetAccessAddress(
    const crashpad::ExceptionSnapshot& exception) {
  if (exception.Exception() != EXC_BAD_ACCESS)
    return 0;

  return exception.ExceptionAddress();
}

}  // namespace internal
}  // namespace gwp_asan
