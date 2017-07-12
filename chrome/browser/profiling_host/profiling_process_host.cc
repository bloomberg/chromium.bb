// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/memlog_sender.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "mojo/edk/embedder/outgoing_broker_client_invitation.h"
#include "mojo/edk/embedder/platform_channel_pair.h"

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
  Launch();
}

ProfilingProcessHost::~ProfilingProcessHost() {
  pph_singleton = nullptr;
}

// static
ProfilingProcessHost* ProfilingProcessHost::EnsureStarted() {
  static ProfilingProcessHost host;
  return &host;
}

// static
ProfilingProcessHost* ProfilingProcessHost::Get() {
  return pph_singleton;
}

// static
void ProfilingProcessHost::AddSwitchesToChildCmdLine(
    base::CommandLine* child_cmd_line) {
  // Watch out: will be called on different threads.
  ProfilingProcessHost* pph = ProfilingProcessHost::Get();
  if (!pph)
    return;
  pph->EnsureControlChannelExists();

  // TODO(brettw) this isn't correct for Posix. Redo when we can shave over
  // Mojo
  child_cmd_line->AppendSwitchASCII(switches::kMemlogPipe, pph->pipe_id_);
}

void ProfilingProcessHost::Launch() {
  mojo::edk::PlatformChannelPair control_channel;
  mojo::edk::HandlePassingInformation handle_passing_info;

// TODO(brettw) most of this logic can be replaced with PlatformChannelPair.

#if defined(OS_WIN)
  base::Process process = base::Process::Current();
  pipe_id_ = base::IntToString(static_cast<int>(process.Pid()));
#else

  // Create the socketpair for the low level memlog pipe.
  // TODO(ajwong): Should this use base/posix/unix_domain_socket_linux.h?
  int memlog_fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, memlog_fds) == 0);
  PCHECK(fcntl(memlog_fds[0], F_SETFL, O_NONBLOCK) == 0);
  PCHECK(fcntl(memlog_fds[1], F_SETFL, O_NONBLOCK) == 0);

  // Store one end for our message sender to use.
  base::ScopedFD child_end(memlog_fds[1]);
  // TODO(brettw) need to get rid of pipe_id when we can share over Mojo.
  pipe_id_ = base::IntToString(memlog_fds[0]);

  handle_passing_info.emplace_back(child_end.get(), child_end.get());
#endif
  base::CommandLine profiling_cmd =
      MakeProfilingCommandLine(base::IntToString(child_end.get()));

  // Keep the server handle, pass the client handle to the child.
  pending_control_connection_ = control_channel.PassServerHandle();
  control_channel.PrepareToPassClientHandleToChildProcess(&profiling_cmd,
                                                          &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
  options.kill_on_parent_death = true;
#else
#error Unsupported OS.
#endif

  process_ = base::LaunchProcess(profiling_cmd, options);
  StartMemlogSender(pipe_id_);
}

void ProfilingProcessHost::EnsureControlChannelExists() {
  // May get called on different threads, we need to be on the IO thread to
  // work.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&ProfilingProcessHost::EnsureControlChannelExists,
                           base::Unretained(this)));
    return;
  }

  if (pending_control_connection_.is_valid())
    ConnectControlChannelOnIO();
}

// This must be called before the client attempts to connect to the control
// pipe.
void ProfilingProcessHost::ConnectControlChannelOnIO() {
  mojo::edk::OutgoingBrokerClientInvitation invitation;
  mojo::ScopedMessagePipeHandle control_pipe =
      invitation.AttachMessagePipe(kProfilingControlPipeName);

  invitation.Send(
      process_.Handle(),
      mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                  std::move(pending_control_connection_)));
  profiling_control_.Bind(
      mojom::ProfilingControlPtrInfo(std::move(control_pipe), 0));

  StartProfilingMojo();
}

}  // namespace profiling
