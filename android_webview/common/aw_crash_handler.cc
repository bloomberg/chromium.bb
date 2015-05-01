// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_crash_handler.h"

#include <android/log.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/logging.h"
#include "build/build_config.h"

namespace {

const int kExceptionSignals[] = {
  SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS
};

struct sigaction old_handlers[arraysize(kExceptionSignals)];

bool crash_handler_registered;

std::string g_crash_msg;

const char* g_crash_msg_ptr;  // Avoid invoking STL magic in a signal handler.

void AwExceptionHandler(int sig, siginfo_t* info, void* uc) {
  if (g_crash_msg_ptr != NULL)
    __android_log_write(ANDROID_LOG_ERROR, "chromium", g_crash_msg_ptr);

  // Detect if some buggy code in the embedder did reinstall the handler using
  // signal() instead of sigaction() (which would cause |info| to be invalid).
  struct sigaction cur_handler;
  if (sigaction(sig, NULL, &cur_handler) != 0 ||
      (cur_handler.sa_flags & SA_SIGINFO) == 0) {
    info = NULL;
  }

  // We served our purpose. Now restore the old crash handlers. If the embedder
  // did register a custom crash handler, it will be invoked by the kernel after
  // this function returns. Otherwise, this will end up invoking the default
  // signal disposition.
  for (uint32_t i = 0; i < arraysize(kExceptionSignals); ++i) {
    if (sigaction(kExceptionSignals[i], &old_handlers[i], NULL) == -1) {
      signal(kExceptionSignals[i], SIG_DFL);
    }
  }

  if ((info != NULL && SI_FROMUSER(info)) || sig == SIGABRT) {
    // This signal was triggered by somebody sending us the signal with kill().
    // In order to retrigger it, we have to queue a new signal by calling
    // kill() ourselves.  The special case (si_pid == 0 && sig == SIGABRT) is
    // due to the kernel sending a SIGABRT from a user request via SysRQ.
    if (syscall(__NR_tgkill, getpid(), syscall(__NR_gettid), sig) < 0) {
      // If we failed to kill ourselves resort to terminating uncleanly.
      exit(1);
    }
  }
}

}  // namespace

namespace android_webview {
namespace crash_handler {

void RegisterCrashHandler(const std::string& version) {
#if defined(ARCH_CPU_X86_FAMILY)
  // Don't install signal handler on X86/64 because this breaks binary
  // translators that handle SIGSEGV in userspace and get chained after our
  // handler. See crbug.com/477444
  return;
#endif

  if (crash_handler_registered) {
    NOTREACHED();
    return;
  }

  g_crash_msg = "### WebView " + version;
  g_crash_msg_ptr = g_crash_msg.c_str();

  // Fail if unable to store all the old handlers.
  for (uint32_t i = 0; i < arraysize(kExceptionSignals); ++i) {
    if (sigaction(kExceptionSignals[i], NULL, &old_handlers[i]) == -1) {
      LOG(ERROR) << "Error while trying to retrieve old handler for signal "
                 << kExceptionSignals[i] << ")";
      return;
    }
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);

  // Mask all exception signals when we're handling one of them.
  for (uint32_t i = 0; i < arraysize(kExceptionSignals); ++i)
    sigaddset(&sa.sa_mask, kExceptionSignals[i]);

  sa.sa_sigaction = AwExceptionHandler;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

  for (uint32_t i = 0; i < arraysize(kExceptionSignals); ++i) {
    if (sigaction(kExceptionSignals[i], &sa, NULL) == -1) {
      // At this point it is impractical to back out changes, and so failure to
      // install a signal is intentionally ignored.
      LOG(ERROR) << "Error while overriding handler for signal "
                 << kExceptionSignals[i];
    }
  }

  crash_handler_registered = true;
}

}  // namespace crash_handler
}  // namespace android_webview
