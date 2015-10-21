// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iostream>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/trace_config_file.h"
#include "components/tracing/tracing_switches.h"
#include "mandoline/app/desktop/launcher_process.h"
#include "mojo/runner/context.h"
#include "mojo/runner/switches.h"
#include "mojo/runner/tracer.h"

namespace mandoline {

int LauncherProcessMain(int argc, char** argv) {
  mojo::runner::Tracer tracer;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  bool trace_startup = command_line.HasSwitch(switches::kTraceStartup);
  if (trace_startup) {
    tracer.Start(
        command_line.GetSwitchValueASCII(switches::kTraceStartup),
        command_line.GetSwitchValueASCII(switches::kTraceStartupDuration),
        "mandoline.trace");
  }

  // We want the runner::Context to outlive the MessageLoop so that pipes are
  // all gracefully closed / error-out before we try to shut the Context down.
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  mojo::runner::Context shell_context(shell_dir, &tracer);
  {
    base::MessageLoop message_loop;
    tracer.DidCreateMessageLoop();
    if (!shell_context.Init()) {
      return 0;
    }

    message_loop.PostTask(FROM_HERE,
                          base::Bind(&mojo::runner::Context::Run,
                                     base::Unretained(&shell_context),
                                     GURL("mojo:desktop_ui")));
    message_loop.Run();

    // Must be called before |message_loop| is destroyed.
    shell_context.Shutdown();
  }

  if (trace_startup)
    tracer.StopAndFlushToFile();

  return 0;
}

}  // namespace mandoline
