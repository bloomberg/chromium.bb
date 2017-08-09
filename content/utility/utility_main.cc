// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/utility/utility_thread_impl.h"

#if defined(OS_LINUX)
#include "content/common/sandbox_linux/sandbox_linux.h"
#endif

#if defined(OS_WIN)
#include "base/rand_util.h"
#include "sandbox/win/src/sandbox.h"
#endif

namespace content {

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  base::MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrUtilityMain");

  if (parameters.command_line.HasSwitch(switches::kUtilityStartupDialog))
    WaitForDebugger("Utility");

#if defined(OS_LINUX)
  // Initializes the sandbox before any threads are created.
  // TODO(jorgelo): move this after GTK initialization when we enable a strict
  // Seccomp-BPF policy.
  if (parameters.zygote_child)
    LinuxSandbox::InitializeSandbox();
#endif

  ChildProcess utility_process;
  utility_process.set_main_thread(new UtilityThreadImpl());

  // Both utility process and service utility process would come
  // here, but the later is launched without connection to service manager, so
  // there has no base::PowerMonitor be created(See ChildThreadImpl::Init()).
  // As base::PowerMonitor is necessary to base::HighResolutionTimerManager, for
  // such case we just disable base::HighResolutionTimerManager for now.
  // Note that disabling base::HighResolutionTimerManager means high resolution
  // timer is always disabled no matter on battery or not, but it should have
  // no any bad influence because currently service utility process is not using
  // any high resolution timer.
  // TODO(leonhsl): Once http://crbug.com/646833 got resolved, re-enable
  // base::HighResolutionTimerManager here for future possible usage of high
  // resolution timer in service utility process.
  base::Optional<base::HighResolutionTimerManager> hi_res_timer_manager;
  if (base::PowerMonitor::Get()) {
    hi_res_timer_manager.emplace();
  }

#if defined(OS_WIN)
  bool no_sandbox = parameters.command_line.HasSwitch(switches::kNoSandbox);
  if (!no_sandbox) {
    sandbox::TargetServices* target_services =
        parameters.sandbox_info->target_services;
    if (!target_services)
      return false;
    char buffer;
    // Ensure RtlGenRandom is warm before the token is lowered; otherwise,
    // base::RandBytes() will CHECK fail when v8 is initialized.
    base::RandBytes(&buffer, sizeof(buffer));
    target_services->LowerToken();
  }
#endif

  base::RunLoop().Run();

#if defined(LEAK_SANITIZER)
  // Invoke LeakSanitizer before shutting down the utility thread, to avoid
  // reporting shutdown-only leaks.
  __lsan_do_leak_check();
#endif

  return 0;
}

}  // namespace content
