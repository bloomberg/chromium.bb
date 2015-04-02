// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iostream>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/command_line_util.h"
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/switches.h"

namespace {

void Usage() {
  std::cerr << "Launch Mojo applications.\n";
  std::cerr
      << "Usage: mojo_shell"
      << " [--" << switches::kArgsFor << "=<mojo-app>]"
      << " [--" << switches::kContentHandlers << "=<handlers>]"
      << " [--" << switches::kEnableExternalApplications << "]"
      << " [--" << switches::kDisableCache << "]"
      << " [--" << switches::kEnableMultiprocess << "]"
      << " [--" << switches::kOrigin << "=<url-lib-path>]"
      << " [--" << switches::kTraceStartup << "]"
      << " [--" << switches::kURLMappings << "=from1=to1,from2=to2]"
      << " [--" << switches::kPredictableAppFilenames << "]"
      << " [--" << switches::kWaitForDebugger << "]"
      << " <mojo-app> ...\n\n"
      << "A <mojo-app> is a Mojo URL or a Mojo URL and arguments within "
      << "quotes.\n"
      << "Example: mojo_shell \"mojo:js_standalone test.js\".\n"
      << "<url-lib-path> is searched for shared libraries named by mojo URLs.\n"
      << "The value of <handlers> is a comma separated list like:\n"
      << "text/html,mojo:html_viewer,"
      << "application/javascript,mojo:js_content_handler\n";
}

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

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  // TODO(vtl): Unify parent and child process cases to the extent possible.
  if (scoped_ptr<mojo::shell::ChildProcess> child_process =
          mojo::shell::ChildProcess::Create(
              *base::CommandLine::ForCurrentProcess())) {
    child_process->Main();
  } else {
    // Only check the command line for the main process.
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();

    const std::set<std::string> all_switches = switches::GetAllSwitches();
    const base::CommandLine::SwitchMap switches = command_line.GetSwitches();
    bool found_unknown_switch = false;
    for (const auto& s : switches) {
      if (all_switches.find(s.first) == all_switches.end()) {
        std::cerr << "unknown switch: " << s.first << std::endl;
        found_unknown_switch = true;
      }
    }

    if (found_unknown_switch ||
        (!command_line.HasSwitch(switches::kEnableExternalApplications) &&
         (command_line.HasSwitch(switches::kHelp) ||
          command_line.GetArgs().empty()))) {
      Usage();
      return 0;
    }

    if (command_line.HasSwitch(switches::kTraceStartup)) {
      g_tracing = true;
      base::trace_event::CategoryFilter category_filter(
          command_line.GetSwitchValueASCII(switches::kTraceStartup));
      base::trace_event::TraceLog::GetInstance()->SetEnabled(
          category_filter, base::trace_event::TraceLog::RECORDING_MODE,
          base::trace_event::TraceOptions(
              base::trace_event::RECORD_UNTIL_FULL));
    }

    // We want the shell::Context to outlive the MessageLoop so that pipes are
    // all gracefully closed / error-out before we try to shut the Context down.
    mojo::shell::Context shell_context;
    {
      base::MessageLoop message_loop;
      if (!shell_context.Init()) {
        Usage();
        return 0;
      }
      if (g_tracing) {
        message_loop.PostDelayedTask(FROM_HERE,
                                     base::Bind(StopTracingAndFlushToDisk),
                                     base::TimeDelta::FromSeconds(5));
      }

      // The mojo_shell --args-for command-line switch is handled specially
      // because it can appear more than once. The base::CommandLine class
      // collapses multiple occurrences of the same switch.
      for (int i = 1; i < argc; i++) {
        ApplyApplicationArgs(&shell_context, argv[i]);
      }

      message_loop.PostTask(
          FROM_HERE,
          base::Bind(&mojo::shell::RunCommandLineApps, &shell_context));
      message_loop.Run();

      // Must be called before |message_loop| is destroyed.
      shell_context.Shutdown();
    }
  }

  if (g_tracing)
    StopTracingAndFlushToDisk();
  return 0;
}
