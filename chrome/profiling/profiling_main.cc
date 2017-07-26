// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_main.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "chrome/profiling/profiling_globals.h"
#include "chrome/profiling/profiling_process.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/incoming_broker_client_invitation.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/edk/embedder/transport_protocol.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace profiling {

int ProfilingMain(const base::CommandLine& cmdline) {
  ProfilingGlobals globals;

  mojo::edk::Init();
  mojo::edk::ScopedIPCSupport ipc_support(
      globals.GetIORunner(),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  // Connect the browser memlog pipe passed on the command line.
  int pipe_int = 0;
  base::StringToInt(cmdline.GetSwitchValueASCII(switches::kMemlogPipe),
                    &pipe_int);
#if defined(OS_WIN)
  ULONG browser_pid = 0;
  HANDLE pipe_handle = reinterpret_cast<HANDLE>(pipe_int);
  ::GetNamedPipeClientProcessId(pipe_handle, &browser_pid);
  globals.GetMemlogConnectionManager()->OnNewConnection(
      scoped_refptr<MemlogReceiverPipe>(new MemlogReceiverPipe(pipe_handle)),
      static_cast<int>(browser_pid));
#else
  // TODO(brettw) this uses 0 for the browser PID, figure this out on Posix.
  globals.GetMemlogConnectionManager()->OnNewConnection(
      scoped_refptr<MemlogReceiverPipe>(new MemlogReceiverPipe(pipe_int)), 0);
#endif

  globals.RunMainMessageLoop();

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif
  return 0;
}

}  // namespace profiling
