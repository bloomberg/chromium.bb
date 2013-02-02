// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/common/dump_without_crashing.h"

#include "base/logging.h"
#include "chrome/common/chrome_constants.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

#if defined(USE_LINUX_BREAKPAD) || defined(OS_MACOSX)
// Pointer to the function that's called by DumpWithoutCrashing() to dump the
// process's memory.
void (*dump_without_crashing_function_)() = NULL;
#endif

}  // namespace

namespace logging {

void DumpWithoutCrashing() {
#if defined(OS_WIN)
  // Get the breakpad pointer from chrome.exe
  typedef void (__cdecl *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(chrome::kBrowserProcessExecutableName),
                       "DumpProcess"));
  if (DumpProcess)
    DumpProcess();
#elif defined(USE_LINUX_BREAKPAD) || defined(OS_MACOSX)
  if (dump_without_crashing_function_)
    (*dump_without_crashing_function_)();
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(USE_LINUX_BREAKPAD) || defined(OS_MACOSX)
void SetDumpWithoutCrashingFunction(void (*function)()) {
  dump_without_crashing_function_ = function;
}
#endif

}  // namespace logging
