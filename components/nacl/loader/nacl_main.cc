// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/timer/hi_res_timer_manager.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/nacl_main_platform_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

// main() routine for the NaCl loader process.
int NaClMain(const content::MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line;

  // The main thread of the plugin services IO.
  base::MessageLoopForIO main_message_loop;
  base::PlatformThread::SetName("CrNaClMain");

  scoped_ptr<base::PowerMonitorSource> power_monitor_source(
      new base::PowerMonitorDeviceSource());
  base::PowerMonitor power_monitor(power_monitor_source.Pass());
  base::HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  NaClMainPlatformDelegate platform(parameters);

  platform.PlatformInitialize();
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  platform.InitSandboxTests(no_sandbox);

#if defined(OS_POSIX)
  // The number of cores must be obtained before the invocation of
  // platform.EnableSandbox(), so cannot simply be inlined below.
  int number_of_cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

  if (!no_sandbox) {
    platform.EnableSandbox();
  }
  bool sandbox_test_result = platform.RunSandboxTests();

  if (sandbox_test_result) {
    NaClListener listener;
#if defined(OS_POSIX)
    listener.set_number_of_cores(number_of_cores);
#endif
    listener.Listen();
  } else {
    // This indirectly prevents the test-harness-success-cookie from being set,
    // as a way of communicating test failure, because the nexe won't reply.
    // TODO(jvoung): find a better way to indicate failure that doesn't
    // require waiting for a timeout.
    VLOG(1) << "Sandbox test failed: Not launching NaCl process";
  }
#else
  NOTIMPLEMENTED() << " not implemented startup, plugin startup dialog etc.";
#endif

  platform.PlatformUninitialize();
  return 0;
}
