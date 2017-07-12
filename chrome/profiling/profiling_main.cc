// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_main.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/profiling_constants.h"
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

  std::string pipe_id = cmdline.GetSwitchValueASCII(switches::kMemlogPipe);
  globals.GetMemlogConnectionManager()->StartConnections(pipe_id);

  globals.RunMainMessageLoop();

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif
  return 0;
}

}  // namespace profiling
