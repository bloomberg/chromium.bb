// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include "build/build_config.h"

// This whole file is only useful on 64-bit architectures.
#if defined(ARCH_CPU_64_BITS)

namespace {

// Signal handler for SIGILL; see WorkaroundFlashLAHF().
void SignalHandler(int signum, siginfo_t* info, void* void_context) {
  const char kLAHFInstruction = 0x9f;
  ucontext_t* context = static_cast<ucontext_t*>(void_context);
  greg_t* regs = context->uc_mcontext.gregs;
  char instruction = *reinterpret_cast<char*>(regs[REG_RIP]);

  // Check whether this is the kind of SIGILL we care about.
  // (info->si_addr can be NULL when we get a SIGILL via other means,
  // like with kill.)
  if (signum != SIGILL || instruction != kLAHFInstruction) {
    // Not the problem we're interested in.  Reraise the signal.  We
    // need to be careful to handle threads etc. properly.

    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    sigaction(signum, &sa, NULL);

    // block the current signal
    sigset_t block_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, signum);
    sigprocmask(SIG_BLOCK, &block_set, NULL);

    // Re-raise signal. It won't be delivered until we return.
    syscall(SYS_tkill, syscall(SYS_gettid), signum);
    return;
  }

  // LAHF moves the low byte of the EFLAGS register to AH.  Emulate that.
  reinterpret_cast<char*>(&regs[REG_RAX])[1] =
      reinterpret_cast<char*>(&regs[REG_EFL])[0];
  // And advance the instruction pointer past the (one-byte) instruction.
  ++regs[REG_RIP];
}

}  // namespace

// 64-bit Flash sometimes uses the LAHF instruction which isn't
// available on some CPUs.  We can work around it by catching SIGILL
// (illegal instruction), checking if the signal was caused by this
// particular circumstance, emulating the instruction, and resuming.
// This function registers the signal handler.
void WorkaroundFlashLAHF() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = &SignalHandler;

  sigaction(SIGILL, &action, NULL);
}

#endif  // defined(ARCH_CPU_64_BITS)
