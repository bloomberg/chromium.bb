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

namespace mandoline {
namespace {

// Whether we're currently tracing.
bool g_tracing = false;

// Number of tracing blocks written.
uint32_t g_blocks = 0;

// Trace file, if open.
FILE* g_trace_file = nullptr;

void WriteTraceDataCollected(
    base::WaitableEvent* event,
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  if (g_blocks) {
    fwrite(",", 1, 1, g_trace_file);
  }

  ++g_blocks;
  fwrite(events_str->data().c_str(), 1, events_str->data().length(),
         g_trace_file);
  if (!has_more_events) {
    static const char kEnd[] = "]}";
    fwrite(kEnd, 1, strlen(kEnd), g_trace_file);
    PCHECK(fclose(g_trace_file) == 0);
    g_trace_file = nullptr;
    event->Signal();
  }
}

void EndTraceAndFlush(base::WaitableEvent* event) {
  g_trace_file = fopen("mojo_shell.trace", "w+");
  PCHECK(g_trace_file);
  static const char kStart[] = "{\"traceEvents\":[";
  fwrite(kStart, 1, strlen(kStart), g_trace_file);
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
  base::trace_event::TraceLog::GetInstance()->Flush(
      base::Bind(&WriteTraceDataCollected, base::Unretained(event)));
}

void StopTracingAndFlushToDisk() {
  g_tracing = false;
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
  base::WaitableEvent flush_complete_event(false, false);
  // TraceLog::Flush requires a message loop but we've already shut ours down.
  // Spin up a new thread to flush things out.
  base::Thread flush_thread("mojo_shell_trace_event_flush");
  flush_thread.Start();
  flush_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(EndTraceAndFlush, base::Unretained(&flush_complete_event)));
  flush_complete_event.Wait();
}

}  // namespace

int LauncherProcessMain(int argc, char** argv) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTraceStartup)) {
    g_tracing = true;
    base::trace_event::TraceConfig trace_config(
        command_line.GetSwitchValueASCII(switches::kTraceStartup),
        base::trace_event::RECORD_UNTIL_FULL);
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
  } else if (tracing::TraceConfigFile::GetInstance()->IsEnabled()) {
    g_tracing = true;
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        tracing::TraceConfigFile::GetInstance()->GetTraceConfig(),
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  // We want the runner::Context to outlive the MessageLoop so that pipes are
  // all gracefully closed / error-out before we try to shut the Context down.
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  mojo::runner::Context shell_context(shell_dir);
  {
    base::MessageLoop message_loop;
    if (!shell_context.Init()) {
      return 0;
    }
    if (g_tracing) {
      message_loop.PostDelayedTask(FROM_HERE,
                                   base::Bind(StopTracingAndFlushToDisk),
                                   base::TimeDelta::FromSeconds(5));
    }

    message_loop.PostTask(FROM_HERE,
                          base::Bind(&mojo::runner::Context::Run,
                                     base::Unretained(&shell_context),
                                     GURL("mojo:desktop_ui")));
    message_loop.Run();

    // Must be called before |message_loop| is destroyed.
    shell_context.Shutdown();
  }

  if (g_tracing)
    StopTracingAndFlushToDisk();
  return 0;
}

}  // namespace mandoline
