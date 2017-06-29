// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/memlog_sender.h"
#include "content/public/common/content_switches.h"

namespace profiling {

namespace {

ProfilingProcessHost* pph_singleton = nullptr;

base::CommandLine MakeProfilingCommandLine(const std::string& pipe_id) {
  // Program name.
  base::FilePath child_path;
#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  // When running under Valgrind, forking /proc/self/exe ends up forking the
  // Valgrind executable, which then crashes. However, it's almost safe to
  // assume that the updates won't happen while testing with Valgrind tools.
  if (!RunningOnValgrind())
    child_path = base::FilePath(base::kProcSelfExe);
#endif
  if (child_path.empty())
    base::PathService::Get(base::FILE_EXE, &child_path);
  base::CommandLine result(child_path);

  result.AppendSwitchASCII(switches::kProcessType, switches::kProfiling);
  result.AppendSwitchASCII(switches::kMemlogPipe, pipe_id);

#if defined(OS_WIN)
  // Windows needs prefetch arguments.
  result.AppendArg(switches::kPrefetchArgumentOther);
#endif

  return result;
}

}  // namespace

ProfilingProcessHost::ProfilingProcessHost() {
  pph_singleton = this;
}

ProfilingProcessHost::~ProfilingProcessHost() {
  pph_singleton = nullptr;
}

// static
ProfilingProcessHost* ProfilingProcessHost::EnsureStarted() {
  static ProfilingProcessHost host;
  if (!host.process_.IsValid())
    host.Launch();
  return &host;
}

// static
ProfilingProcessHost* ProfilingProcessHost::Get() {
  return pph_singleton;
}

// static
void ProfilingProcessHost::AddSwitchesToChildCmdLine(
    base::CommandLine* child_cmd_line) {
  ProfilingProcessHost* pph = ProfilingProcessHost::Get();
  if (!pph)
    return;
  child_cmd_line->AppendSwitchASCII(switches::kMemlogPipe, pph->pipe_id_);
}

void ProfilingProcessHost::Launch() {
  base::Process process = base::Process::Current();
  pipe_id_ = base::IntToString(static_cast<int>(process.Pid()));

  base::CommandLine profiling_cmd = MakeProfilingCommandLine(pipe_id_);

  base::LaunchOptions options;
  process_ = base::LaunchProcess(profiling_cmd, options);
  StartMemlogSender(pipe_id_);
}

}  // namespace profiling
