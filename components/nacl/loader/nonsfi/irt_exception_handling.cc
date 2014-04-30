// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_exception.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

namespace nacl {
namespace nonsfi {
namespace {

// This is NonSFI version of exception handling codebase, NaCl side of
// things resides in:
// native_client/src/trusted/service_runtime/linux/nacl_signal.c
// native_client/src/trusted/service_runtime/sys_exception.c

// Crash signals to handle.  The differences from SFI NaCl are that
// NonSFI NaCl does not use NACL_THREAD_SUSPEND_SIGNAL (==SIGUSR1),
// and SIGSYS is reserved for seccomp-bpf.
const int kSignals[] = {
  SIGSTKFLT,
  SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV,
  // Handle SIGABRT in case someone sends it asynchronously using kill().
  SIGABRT
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
NaClExceptionHandler signal_handler_function_pointer = NULL;

// Signal handler, responsible for calling the registered handler.
void SignalCatch(int sig, siginfo_t* info, void* uc) {
  if (signal_handler_function_pointer) {
    NaClSignalContext signal_context;
    NaClSignalContextFromHandler(&signal_context, uc);
    NaClExceptionFrame exception_frame;
    NaClSignalSetUpExceptionFrame(&exception_frame,
                                  &signal_context,
                                  0 /* context_user_addr,
                                       not useful for NonSFI NaCl. */);
    signal_handler_function_pointer(&exception_frame.context);
  }
  _exit(-1);
}

int IrtExceptionHandler(NaClExceptionHandler handler,
                               NaClExceptionHandler* old_handler) {
  pthread_mutex_lock(&mutex);
  if (old_handler)
    *old_handler = signal_handler_function_pointer;
  signal_handler_function_pointer = handler;
  pthread_mutex_unlock(&mutex);
  return 0;
}

int IrtExceptionStack(void* stack, size_t size) {
  // TODO(uekawa): Implement this function so that the exception stack
  // actually gets used for running an exception handler.  Currently
  // we don't switch stack, which means we can't handle stack overflow
  // exceptions.
  return 0;
}

int IrtExceptionClearFlag(void) {
  // TODO(uekawa): Implement clear_flag() to behave like SFI NaCl's
  // implementation, so that a thread can handle a second exception
  // after handling a first exception
  return ENOSYS;
}

}  // namespace

const struct nacl_irt_exception_handling kIrtExceptionHandling = {
  IrtExceptionHandler,
  IrtExceptionStack,
  IrtExceptionClearFlag,
};

void InitializeSignalHandler() {
  struct sigaction sa;
  unsigned int a;

  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = SignalCatch;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

  // Mask all signals we catch to prevent re-entry.
  for (a = 0; a < NACL_ARRAY_SIZE(kSignals); a++) {
    sigaddset(&sa.sa_mask, kSignals[a]);
  }

  // Install all handlers.
  for (a = 0; a < NACL_ARRAY_SIZE(kSignals); a++) {
    if (sigaction(kSignals[a], &sa, NULL) != 0)
      NaClLog(LOG_FATAL, "sigaction to register signals failed.\n");
  }
}

}  // namespace nonsfi
}  // namespace nacl
