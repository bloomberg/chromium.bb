// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/plugin/plugin_thread.h"

#if defined(OS_WIN)
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"
#elif defined(OS_LINUX)
#include "chrome/common/chrome_descriptors.h"
#include "base/global_descriptors_posix.h"
#endif

#if defined(OS_MACOSX)

// To support Mac NPAPI plugins that use the Carbon event model (i.e., most
// shipping plugins for MacOS X 10.5 and earlier), we need some way for the
// Carbon event dispatcher to run, even though the plugin host process itself
// does not use Carbon events.  Rather than give control to the standard
// Carbon event loop, we schedule a periodic task on the main thread which
// empties the Carbon event queue every 20ms (chosen to match how often Safari
// does the equivalent operation).  This allows plugins to receive idle events
// and schedule Carbon timers without swamping the CPU.  If, in the future,
// we remove support for the Carbon event model and only support the Cocoa
// event model, this can be removed.  Note that this approach does not install
// standard application event handlers for the menubar, AppleEvents, and so on.
// This is intentional, since the plugin process is not actually an application
// with its own UI elements--all rendering and event handling happens via IPC
// to the renderer process which invoked it.

namespace {

const int kPluginUpdateIntervalMs = 20; // 20ms = 50Hz

void PluginCarbonEventTask() {
  EventRef theEvent;
  EventTargetRef theTarget;

  theTarget = GetEventDispatcherTarget();

  // Dispatch any pending events. but do not block if there are no events.
  while (ReceiveNextEvent(0, NULL, kEventDurationNoWait,
                          true, &theEvent) == noErr) {
    SendEventToEventTarget (theEvent, theTarget);
    ReleaseEvent(theEvent);
  }

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(PluginCarbonEventTask), kPluginUpdateIntervalMs);
}

}
#endif

// main() routine for running as the plugin process.
int PluginMain(const MainFunctionParams& parameters) {
  // The main thread of the plugin services IO.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_PluginMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  const CommandLine& parsed_command_line = parameters.command_line_;

#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();

  CoInitialize(NULL);
  DLOG(INFO) << "Started plugin with " <<
    parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox) ||
                    !parsed_command_line.HasSwitch(switches::kSafePlugins);
  if (target_services && !no_sandbox) {
    // The command line might specify a test plugin to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_plugin_name =
          parsed_command_line.GetSwitchValue(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_plugin_name.c_str());
      DCHECK(sandbox_test_module);
    }
  }
#endif
  if (parsed_command_line.HasSwitch(switches::kPluginStartupDialog)) {
#if defined(OS_WIN)
    std::wstring title = chrome::kBrowserAppName;
    title += L" plugin";  // makes attaching to process easier
    win_util::MessageBox(NULL, L"plugin starting...", title,
                         MB_OK | MB_SETFOREGROUND);
#elif defined(OS_MACOSX)
    // TODO(playmobil): In the long term, overriding this flag doesn't seem
    // right, either use our own flag or open a dialog we can use.
    // This is just to ease debugging in the interim.
    LOG(WARNING) << "Plugin ("
    << getpid()
    << ") paused waiting for debugger to attach @ pid";
    pause();
#else
  NOTIMPLEMENTED() << " non-windows startup, plugin startup dialog etc.";
#endif
  }

#if defined(OS_MACOSX)
  // Spin off a consumer for the native (Carbon) event stream so
  // that plugin timers, event handlers, etc. will work properly.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(PluginCarbonEventTask), kPluginUpdateIntervalMs);
#endif

  {
    ChildProcess plugin_process(new PluginThread());
#if defined(OS_WIN)
    if (!no_sandbox && target_services)
      target_services->LowerToken();

    if (sandbox_test_module) {
      RunRendererTests run_security_tests =
          reinterpret_cast<RunPluginTests>(GetProcAddress(sandbox_test_module,
                                                          kPluginTestCall));
      DCHECK(run_security_tests);
      if (run_security_tests) {
        int test_count = 0;
        DLOG(INFO) << "Running plugin security tests";
        BOOL result = run_security_tests(&test_count);
        DCHECK(result) << "Test number " << test_count << " has failed.";
        // If we are in release mode, crash or debug the process.
        if (!result)
          __debugbreak();
      }
    }
#endif

    MessageLoop::current()->Run();
  }

#if defined(OS_WIN)
  CoUninitialize();
#endif

  return 0;
}
