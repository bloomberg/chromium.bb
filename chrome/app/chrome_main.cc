// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/callback_helpers.h"
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

#if BUILDFLAG(ENABLE_OOP_HEAP_PROFILING)
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/common/profiling/memlog_sender.h"
#include "chrome/profiling/profiling_main.h"
#endif

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
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
#include "chrome_elf/chrome_elf_main.h"

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
  // The process should crash when going through abnormal termination, but we
  // must be sure to reset this setting when ChromeMain returns normally.
  auto crash_on_detach_resetter = base::ScopedClosureRunner(
      base::Bind(&base::win::SetShouldCrashOnProcessDetach,
                 base::win::ShouldCrashOnProcessDetach()));
  base::win::SetShouldCrashOnProcessDetach(true);
  base::win::SetAbortBehaviorForCrashReporting();
  params.instance = instance;
  params.sandbox_info = sandbox_info;

  // Pass chrome_elf's copy of DumpProcessWithoutCrash resolved via load-time
  // dynamic linking.
  base::debug::SetDumpWithoutCrashingFunction(&DumpProcessWithoutCrash);

  // Verify that chrome_elf and this module (chrome.dll and chrome_child.dll)
  // have the same version.
  if (install_static::InstallDetails::Get().VersionMismatch())
    base::debug::DumpWithoutCrashing();
#else
  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);
#endif  // defined(OS_WIN)
  base::CommandLine::Init(0, nullptr);
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  ALLOW_UNUSED_LOCAL(command_line);

  // Chrome-specific process modes.
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
  if (command_line->HasSwitch(switches::kHeadless)) {
#if defined(OS_MACOSX)
    SetUpBundleOverrides();
#endif
    return headless::HeadlessShellMain(params);
  }
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

#if BUILDFLAG(ENABLE_OOP_HEAP_PROFILING)
#if !defined(OS_WIN) || defined(COMPONENT_BUILD) || \
    defined(CHROME_MULTIPLE_DLL_BROWSER)
  // The profiling server is only compiled into the browser process. On Windows,
  // non-component builds, browser code is only used for
  // CHROME_MULTIPLE_DLL_BROWSER.
  if (command_line->HasSwitch(switches::kMemlog))
    profiling::ProfilingProcessHost::EnsureStarted();
#endif
#if !defined(OS_WIN) || defined(COMPONENT_BUILD) || \
    defined(CHROME_MULTIPLE_DLL_CHILD)
  // The profiling process is only compiled into the child process. On Windows,
  // non-component builds, child code is only used for
  // CHROME_MULTIPLE_DLL_CHILD.
  //
  // This is a child process type implemented at the
  // Chrome layer rather than the content layer (like the other child procs.).
  if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
      switches::kProfiling)
    return profiling::ProfilingMain(*command_line);
#endif
#endif  // ENABLE_OOP_HEAP_PROFILING

#if defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
  if (service_manager::ServiceManagerIsRemote())
    params.env_mode = aura::Env::Mode::MUS;
#endif  // BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)

  int rv = content::ContentMain(params);

  return rv;
}
