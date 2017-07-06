// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/memlog_sender.h"
#include "content/public/common/content_switches.h"

#if defined(OS_LINUX)
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/process/process_metrics.h"
#include "base/third_party/valgrind/valgrind.h"
#endif

namespace profiling {

namespace {

ProfilingProcessHost* pph_singleton = nullptr;

#if defined(OS_LINUX)
bool IsRunningOnValgrind() {
  return RUNNING_ON_VALGRIND;
}
#endif

base::CommandLine MakeProfilingCommandLine(const std::string& pipe_id) {
  // Program name.
  base::FilePath child_path;
#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  // When running under Valgrind, forking /proc/self/exe ends up forking the
  // Valgrind executable, which then crashes. However, it's almost safe to
  // assume that the updates won't happen while testing with Valgrind tools.
  if (!IsRunningOnValgrind())
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

#if defined(OS_WIN)
void ProfilingProcessHost::Launch() {
  base::Process process = base::Process::Current();
  base::LaunchOptions options;

  pipe_id_ = base::IntToString(static_cast<int>(process.Pid()));
  base::CommandLine profiling_cmd = MakeProfilingCommandLine(pipe_id_);

  process_ = base::LaunchProcess(profiling_cmd, options);
  StartMemlogSender(pipe_id_);
}
#elif defined(OS_POSIX)

void ProfilingProcessHost::Launch() {
  // Create the socketpair.
  // TODO(ajwong): Should this use base/posix/unix_domain_socket_linux.h?
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  PCHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
  PCHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);

  // Store one end for our message sender to use.
  pipe_id_ = base::IntToString(fds[0]);
  base::ScopedFD child_end(fds[1]);

  // This is a new process forked from us. No need to remap.
  base::FileHandleMappingVector fd_map;
  fd_map.emplace_back(std::pair<int, int>(child_end.get(), child_end.get()));

  base::LaunchOptions options;
  options.fds_to_remap = &fd_map;
  options.kill_on_parent_death = true;

  base::CommandLine profiling_cmd =
      MakeProfilingCommandLine(base::IntToString(child_end.get()));
  process_ = base::LaunchProcess(profiling_cmd, options);
  StartMemlogSender(pipe_id_);
}
#else
#error Unsupported OS.
#endif

}  // namespace profiling
