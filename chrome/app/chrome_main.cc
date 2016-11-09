// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "build/build_config.h"
#include "base/time/time.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/features.h"
#include "content/public/app/content_main.h"

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
#include "base/command_line.h"
#include "chrome/app/mash/mash_runner.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#endif

#if defined(OS_WIN)
#include "base/debug/dump_without_crashing.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"

#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_entry_point_ticks);
}
#elif defined(OS_POSIX)
extern "C" {
__attribute__((visibility("default")))
int ChromeMain(int argc, const char** argv);
}
#endif

#if defined(OS_WIN)
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_entry_point_ticks) {
#elif defined(OS_POSIX)
int ChromeMain(int argc, const char** argv) {
  int64_t exe_entry_point_ticks = 0;
#endif
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  // VS2013 only checks the existence of FMA3 instructions, not the enabled-ness
  // of them at the OS level (this is fixed in VS2015). We force off usage of
  // FMA3 instructions in the CRT to avoid using that path and hitting illegal
  // instructions when running on CPUs that support FMA3, but OSs that don't.
  // See http://crbug.com/436603.
  _set_FMA3_enable(0);
#endif  // WIN && ARCH_CPU_X86_64

  ChromeMainDelegate chrome_main_delegate(
      base::TimeTicks::FromInternalValue(exe_entry_point_ticks));
  content::ContentMainParams params(&chrome_main_delegate);

#if defined(OS_WIN)
  // The process should crash when going through abnormal termination.
  base::win::SetShouldCrashOnProcessDetach(true);
  base::win::SetAbortBehaviorForCrashReporting();
  params.instance = instance;
  params.sandbox_info = sandbox_info;

  // SetDumpWithoutCrashingFunction must be passed the DumpProcess function
  // from the EXE and not from the DLL in order for DumpWithoutCrashing to
  // function correctly.
  typedef void (__cdecl *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(chrome::kChromeElfDllName),
                       "DumpProcessWithoutCrash"));
  CHECK(DumpProcess);
  base::debug::SetDumpWithoutCrashingFunction(DumpProcess);
#else
  params.argc = argc;
  params.argv = argv;
#endif

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
#if !defined(OS_WIN)
  base::CommandLine::Init(params.argc, params.argv);
#endif

  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::UNKNOWN) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch("mash"))
      return MashMain();
  }
#endif  // BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)

  int rv = content::ContentMain(params);

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif

  return rv;
}
