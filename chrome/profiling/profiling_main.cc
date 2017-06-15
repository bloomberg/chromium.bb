// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_main.h"

#include "base/command_line.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace profiling {

int ProfilingMain(const base::CommandLine& cmdline) {
  // TODO(brettw) implement this.

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif
  return 0;
}

}  // namespace profiling
