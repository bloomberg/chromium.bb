// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main.h"

#include "base/allocator/allocator_shim.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/notification_service_impl.h"
#include "content/common/child_process.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace {

bool g_exited_main_message_loop = false;

}  // namespace

namespace content {

bool ExitedMainMessageLoop() {
  return g_exited_main_message_loop;
}

}  // namespace content

// Main routine for running as the Browser process.
int BrowserMain(const content::MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN_ETW("BrowserMain", 0, "");

  // ChildProcess:: is a misnomer unless you consider context.  Use
  // of --wait-for-debugger only makes sense when Chrome itself is a
  // child process (e.g. when launched by PyAuto).
  if (parameters.command_line.HasSwitch(switches::kWaitForDebugger))
    ChildProcess::WaitForDebugger("Browser");

  NotificationServiceImpl main_notification_service;

  scoped_ptr<content::BrowserMainLoop> main_loop(
      new content::BrowserMainLoop(parameters));

  main_loop->Init();

  main_loop->EarlyInitialization();

  // Must happen before we try to use a message loop or display any UI.
  main_loop->InitializeToolkit();

  main_loop->MainMessageLoopStart();

  // WARNING: If we get a WM_ENDSESSION, objects created on the stack here
  // are NOT deleted. If you need something to run during WM_ENDSESSION add it
  // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

  // !!!!!!!!!! READ ME !!!!!!!!!!
  // I (viettrungluu) am in the process of refactoring |BrowserMain()|. If you
  // need to add something above this comment, read the documentation in
  // browser_main.h. If you need to add something below, please do the
  // following:
  //  - Figure out where you should add your code. Do NOT just pick a random
  //    location "which works".
  //  - Document the dependencies apart from compile-time-checkable ones. What
  //    must happen before your new code is executed? Does your new code need to
  //    run before something else? Are there performance reasons for executing
  //    your code at that point?
  //  - If you need to create a (persistent) object, heap allocate it and keep a
  //    |scoped_ptr| to it rather than allocating it on the stack. Otherwise
  //    I'll have to convert your code when I refactor.
  //  - Unless your new code is just a couple of lines, factor it out into a
  //    function with a well-defined purpose. Do NOT just add it inline in
  //    |BrowserMain()|.
  // Thanks!

  // TODO(viettrungluu): put the remainder into BrowserMainParts

#if defined(OS_WIN)
#if !defined(NO_TCMALLOC)
  // When linking shared libraries, NO_TCMALLOC is defined, and dynamic
  // allocator selection is not supported.

  // Make this call before going multithreaded, or spawning any subprocesses.
  base::allocator::SetupSubprocessAllocator();
#endif

  base::win::ScopedCOMInitializer com_initializer;
#endif  // OS_WIN

  base::StatisticsRecorder statistics;

  main_loop->RunMainMessageLoopParts(&g_exited_main_message_loop);

  TRACE_EVENT_END_ETW("BrowserMain", 0, 0);

  return main_loop->GetResultCode();
}
