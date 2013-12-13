// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "content/child/child_process.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/utility/utility_thread_impl.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>

#include "ui/gfx/gtk_util.h"
#endif

namespace content {

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  base::MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrUtilityMain");

#if defined(OS_LINUX)
  // Initializes the sandbox before any threads are created.
  // TODO(jorgelo): move this after GTK initialization when we enable a strict
  // Seccomp-BPF policy.
  LinuxSandbox::InitializeSandbox();
#endif

#if defined(OS_POSIX)
  // The utility process is used to load plugins (see OnLoadPlugins() in
  // utility_thread_impl.cc). Some plugins expect the browser to have loaded
  // GLib/GTK.
  // Due to bugs in GLib we need to initialize GLib/GTK before we start threads,
  // see crbug.com/309093.

#if defined(TOOLKIT_GTK)
  bool is_sandboxed = false;

#if defined(OS_LINUX)
  // On Linux, we only initialize GLib/GTK if we're not sandboxed.
  is_sandboxed = !parameters.command_line.HasSwitch(switches::kNoSandbox);
#endif

  if (!is_sandboxed) {
    // g_thread_init() is deprecated since glib 2.31.0, please see release note:
    // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if !(GLIB_CHECK_VERSION(2, 31, 0))
    if (!g_thread_get_initialized()) {
      g_thread_init(NULL);
    }
#endif
    gfx::GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());
  }
#endif
#endif

  ChildProcess utility_process;
  utility_process.set_main_thread(new UtilityThreadImpl());

  base::HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_WIN)
  bool no_sandbox = parameters.command_line.HasSwitch(switches::kNoSandbox);
  if (!no_sandbox) {
    sandbox::TargetServices* target_services =
        parameters.sandbox_info->target_services;
    if (!target_services)
      return false;
    target_services->LowerToken();
  }
#endif

  base::MessageLoop::current()->Run();

#if defined(LEAK_SANITIZER)
  // Invoke LeakSanitizer before shutting down the utility thread, to avoid
  // reporting shutdown-only leaks.
  __lsan_do_leak_check();
#endif

  return 0;
}

}  // namespace content
