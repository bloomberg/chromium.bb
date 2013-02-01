// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DUMP_WITHOUT_CRASHING_H_
#define CHROME_COMMON_DUMP_WITHOUT_CRASHING_H_

#include "build/build_config.h"

namespace logging {

// Handler to silently dump the current process without crashing.
void DumpWithoutCrashing();

#if defined(USE_LINUX_BREAKPAD) || defined(OS_MACOSX)
// Sets a function that'll be invoked to dump the current process when
// DumpWithoutCrashing() is called.
void SetDumpWithoutCrashingFunction(void (*function)());
#endif

}  // namespace logging

#endif  // CHROME_COMMON_DUMP_WITHOUT_CRASHING_H_
