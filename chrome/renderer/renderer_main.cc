// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "grit/generated_resources.h"
#include "net/base/net_module.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleRendererErrorTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kRendererAssertTest)) {
    DCHECK(false);
  }

  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kRendererCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }

  if (command_line.HasSwitch(switches::kRendererStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Renderer");
  }
}

// mainline routine for running as the Renderer process
int RendererMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;
  base::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool_;

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  // Configure the network module so it has access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);

  // This function allows pausing execution using the --renderer-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleRendererErrorTestParameters(parsed_command_line);

  RendererMainPlatformDelegate platform(parameters);

  StatsScope<StatsCounterTimer>
      startup_timer(chrome::Counters::renderer_main());

#if defined(OS_MACOSX)
  // As long as we use Cocoa in the renderer (for the forseeable future as of
  // now; see http://crbug.com/13890 for info) we need to have a UI loop.
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
#else
  // The main message loop of the renderer services doesn't have IO or UI tasks,
  // unless in-process-plugins is used.
  MessageLoop main_message_loop(RenderProcess::InProcessPlugins() ?
              MessageLoop::TYPE_UI : MessageLoop::TYPE_DEFAULT);
#endif

  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_RendererMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  platform.PlatformInitialize();

  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  platform.InitSandboxTests(no_sandbox);

  // Initialize histogram statistics gathering system.
  // Don't create StatisticsRecorder in the single process mode.
  scoped_ptr<StatisticsRecorder> statistics;
  if (!StatisticsRecorder::WasStarted()) {
    statistics.reset(new StatisticsRecorder());
  }

  // Initialize statistical testing infrastructure.
  FieldTrialList field_trial;
  // Ensure any field trials in browser are reflected into renderer.
  if (parsed_command_line.HasSwitch(switches::kForceFieldTestNameAndValue)) {
    std::string persistent(WideToASCII(parsed_command_line.GetSwitchValue(
        switches::kForceFieldTestNameAndValue)));
    bool ret = field_trial.StringAugmentsState(persistent);
    DCHECK(ret);
  }

  {
#if !defined(OS_LINUX)
    // TODO(markus): Check if it is OK to unconditionally move this
    // instruction down.
    RenderProcess render_process;
    render_process.set_main_thread(new RenderThread());
#endif
    bool run_loop = true;
    if (!no_sandbox) {
      run_loop = platform.EnableSandbox();
    }
#if defined(OS_LINUX)
    RenderProcess render_process;
    render_process.set_main_thread(new RenderThread());
#endif

    platform.RunSandboxTests();

    startup_timer.Stop();  // End of Startup Time Measurement.

    if (run_loop) {
      if (pool)
        pool->Recycle();
      MessageLoop::current()->Run();
    }
  }
  platform.PlatformUninitialize();
  return 0;
}
