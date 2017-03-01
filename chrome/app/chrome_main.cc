// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "headless/public/headless_shell.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
#include "chrome/app/mash/mash_runner.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#include "services/service_manager/runner/common/client_util.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/app/chrome_main_mac.h"
#endif

#if defined(OS_WIN)
#include "base/debug/dump_without_crashing.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_details.h"

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

#if defined(OS_WIN)
  install_static::InitializeFromPrimaryModule();
#endif

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
  // from chrome_elf and not from the DLL in order for DumpWithoutCrashing to
  // function correctly.
  typedef void (__cdecl *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(chrome::kChromeElfDllName),
                       "DumpProcessWithoutCrash"));
  CHECK(DumpProcess);
  base::debug::SetDumpWithoutCrashingFunction(DumpProcess);

  // Verify that chrome_elf and this module (chrome.dll and chrome_child.dll)
  // have the same version.
  if (install_static::InstallDetails::Get().VersionMismatch())
    base::debug::DumpWithoutCrashing();
#else
  params.argc = argc;
  params.argv = argv;
#endif

#if !defined(OS_WIN)
  base::CommandLine::Init(params.argc, params.argv);
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  ALLOW_UNUSED_LOCAL(command_line);
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX)
  if (command_line->HasSwitch(switches::kHeadless)) {
#if defined(OS_MACOSX)
    SetUpBundleOverrides();
#endif
    return headless::HeadlessShellMain(argc, argv);
  }
#endif  // defined(OS_LINUX) || defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::UNKNOWN) {
    if (command_line->HasSwitch(switches::kMash))
      return MashMain();
    WaitForMashDebuggerIfNecessary();
    if (service_manager::ServiceManagerIsRemote())
      params.env_mode = aura::Env::Mode::MUS;
  }
#endif  // BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)

  int rv = content::ContentMain(params);

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif

  return rv;
}
