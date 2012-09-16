// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/app/metro_driver_win.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/common/result_codes.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace {

// TODO(jschuh): Remove this after we narrow down the cause of the crashes.
class ThreadTracker {
 public:
  static void NTAPI UpdateCount(PVOID module, DWORD reason, PVOID reserved) {
    if (reason == DLL_THREAD_ATTACH) {
      // We hit the threshold, but the loader would eat an exception fired now.
      // So schedule an APC to fire the exception once loading is done.
      if (::InterlockedIncrement(&count_) > cap_)
        ::QueueUserAPC(CrashProcessCallback, ::GetCurrentThread(), NULL);
    } else if (reason == DLL_THREAD_DETACH) {
      ::InterlockedDecrement(&count_);
    }
  }

  static void SetCap(LONG cap) { cap_ = cap; }

 private:
  static void CALLBACK CrashProcessCallback(ULONG_PTR) { __debugbreak(); }

  static volatile LONG count_;
  static volatile LONG cap_;
};

LONG volatile ThreadTracker::count_ = 1;
LONG volatile ThreadTracker::cap_ = LONG_MAX;

}  // namespace

// Magic required to get our function called on thread attach and detach.
extern "C" {
#pragma data_seg(push, old_seg)
#pragma data_seg(".CRT$XLB")
PIMAGE_TLS_CALLBACK p_thread_callback = ThreadTracker::UpdateCount;
#pragma data_seg(pop, old_seg)

#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_p_thread_callback")
}

int RunChrome(HINSTANCE instance) {
  bool exit_now = true;
  // We restarted because of a previous crash. Ask user if we should relaunch.
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    if (exit_now)
      return content::RESULT_CODE_NORMAL_EXIT;
  }

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);

  // Cap the threads for any sandboxed process.
  if (sandbox_info.target_services)
    ThreadTracker::SetCap(200);

  // Load and launch the chrome dll. *Everything* happens inside.
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance, &sandbox_info);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;
  return rc;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  MetroDriver metro_driver;
  if (metro_driver.in_metro_mode())
    return metro_driver.RunInMetro(instance, &RunChrome);
  // Not in metro mode, proceed as normal.
  return RunChrome(instance);
}
