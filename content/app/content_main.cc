// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/process_util.h"
#include "content/app/content_main_delegate.h"

#if defined(OS_WIN)
#include <atlbase.h>
#include <atlapp.h>
#include <new.h>
#include <malloc.h>
#elif defined(OS_POSIX)
#include <signal.h>

#include "base/global_descriptors_posix.h"
#include "content/common/chrome_descriptors.h"
#endif  // OS_WIN

#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
extern "C" {
int tc_set_new_mode(int mode);
}
#endif

namespace {

base::AtExitManager* g_exit_manager;
base::mac::ScopedNSAutoreleasePool* g_autorelease_pool;

#if defined(OS_WIN)

static CAppModule _Module;

#pragma optimize("", off)
// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
void InvalidParameter(const wchar_t* expression, const wchar_t* function,
                      const wchar_t* file, unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
  _exit(1);
}

void PureCall() {
  __debugbreak();
  _exit(1);
}
#pragma optimize("", on)

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler() {
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  // Also enable the new handler for malloc() based failures.
  _set_new_mode(1);
}

#elif defined(OS_POSIX)

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK(0 == sigemptyset(&empty_signal_set));
  CHECK(0 == sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] =
      {SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
       SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK(0 == sigaction(signals_to_reset[i], &sigact, NULL));
  }

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
}

#endif  // OS_WIN

}  // namespace

namespace content {

#if defined(OS_WIN)
int ContentMain(HINSTANCE instance,
                sandbox::SandboxInterfaceInfo* sandbox_info,
                ContentMainDelegate* delegate) {
  // argc/argv are ignored on Windows; see command_line.h for details.
  int argc = 0;
  char** argv = NULL;

  RegisterInvalidParamHandler();
  _Module.Init(NULL, static_cast<HINSTANCE>(instance));
#else
int ContentMain(int argc,
                char** argv,
                ContentMainDelegate* delegate) {
  // NOTE(willchan): One might ask why this call is done here rather than in
  // process_util_linux.cc with the definition of
  // EnableTerminationOnOutOfMemory().  That's because base shouldn't have a
  // dependency on TCMalloc.  Really, we ought to have our allocator shim code
  // implement this EnableTerminationOnOutOfMemory() function.  Whateverz.  This
  // works for now.
#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
  // For tcmalloc, we need to tell it to behave like new.
  tc_set_new_mode(1);
#endif

  // Set C library locale to make sure CommandLine can parse argument values
  // in correct encoding.
  setlocale(LC_ALL, "");

  SetupSignalHandlers();

  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(kPrimaryIPCChannel,
             kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#if defined(OS_LINUX)
  g_fds->Set(kCrashDumpSignal,
             kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif

#endif  // OS_WIN

  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();

  // The exit manager is in charge of calling the dtors of singleton objects.
  g_exit_manager = new base::AtExitManager();

  // We need this pool for all the objects created before we get to the
  // event loop, but we don't want to leave them hanging around until the
  // app quits. Each "main" needs to flush this pool right before it goes into
  // its main event loop to get rid of the cruft.
  g_autorelease_pool = new base::mac::ScopedNSAutoreleasePool();

  CommandLine::Init(argc, argv);

  int exit_code;
  if (delegate &&
      delegate->BasicStartupComplete(&exit_code, g_autorelease_pool))
    return exit_code;

  // Temporary
  return 0;
}

void ContentMainEnd() {
#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC

  _Module.Term();

  delete g_autorelease_pool;
  delete g_exit_manager;
#endif  // OS_WIN
}

}  // namespace content
