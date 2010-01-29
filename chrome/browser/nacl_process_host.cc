// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/nacl_process_host.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#endif

#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_messages.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_switches.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

NaClProcessHost::NaClProcessHost(
    ResourceDispatcherHost *resource_dispatcher_host,
    const std::wstring& url)
    : ChildProcessHost(NACL_PROCESS, resource_dispatcher_host),
      resource_dispatcher_host_(resource_dispatcher_host),
      reply_msg_(NULL),
      descriptor_(0) {
  set_name(url);
}

NaClProcessHost::~NaClProcessHost() {
  if (!reply_msg_)
    return;

  // OnProcessLaunched didn't get called because the process couldn't launch.
  // Don't keep the renderer hanging.
  reply_msg_->set_reply_error();
  resource_message_filter_->Send(reply_msg_);
}

bool NaClProcessHost::Launch(ResourceMessageFilter* resource_message_filter,
                             const int descriptor,
                             IPC::Message* reply_msg) {
#ifdef DISABLE_NACL
  NOTIMPLEMENTED() << "Native Client disabled at build time";
  return false;
#else

  // Create a connected socket
  if (nacl::SocketPair(pair_) == -1)
    return false;

  // Launch the process
  descriptor_ = descriptor;
  if (!LaunchSelLdr()) {
    nacl::Close(pair_[0]);
    return false;
  }

  resource_message_filter_ = resource_message_filter;
  reply_msg_ = reply_msg;

  return true;
#endif  // DISABLE_NACL
}

bool NaClProcessHost::LaunchSelLdr() {
  if (!CreateChannel())
    return false;

  // Build command line for nacl.
  FilePath exe_path = GetChildPath(true);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  if (logging::DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  // Propagate the following switches to the plugin command line (along with
  // any associated values) if present in the browser command line.
  // TODO(gregoryd): check which flags of those below can be supported.
  static const char* const switch_names[] = {
    switches::kNoSandbox,
    switches::kTestSandbox,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kMemoryProfiling,
#if defined(OS_MACOSX)
    // TODO(dspringer): remove this when NaCl x86-32 security issues are fixed
    switches::kEnableNaClOnMac,
#endif
  };

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

#if defined(OS_MACOSX)
// TODO(dspringer): NaCl is temporalrily disabled on the Mac by default, but it
// can be enabled with the --enable-nacl cmd-line switch.  Remove this check
// when the security issues in the Mac PIC code are resolved.
  if (!browser_command_line.HasSwitch(switches::kEnableNaClOnMac))
    return false;
#endif

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      cmd_line->AppendSwitchWithValue(switch_names[i],
          browser_command_line.GetSwitchValueASCII(switch_names[i]));
    }
  }

  cmd_line->AppendSwitchWithValue(switches::kProcessType,
                                  switches::kNaClProcess);

  cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
                                  ASCIIToWide(channel_id()));

  ChildProcessHost::Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      false,
      base::environment_vector(),
#endif
      cmd_line);

  return true;
}

void NaClProcessHost::OnProcessLaunched() {
  nacl::FileDescriptor imc_handle;
  base::ProcessHandle nacl_process_handle;
#if NACL_WINDOWS
  // Duplicate the IMC handle
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  reinterpret_cast<HANDLE>(pair_[0]),
                  resource_message_filter_->handle(),
                  &imc_handle,
                  GENERIC_READ | GENERIC_WRITE,
                  FALSE,
                  DUPLICATE_CLOSE_SOURCE);

  // Duplicate the process handle
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  handle(),
                  resource_message_filter_->handle(),
                  &nacl_process_handle,
                  PROCESS_DUP_HANDLE,
                  FALSE,
                  0);

#else
  int flags = fcntl(pair_[0], F_GETFD);
  if (flags != -1) {
    flags |= FD_CLOEXEC;
    fcntl(pair_[0], F_SETFD, flags);
  }
  // No need to dup the imc_handle - we don't pass it anywhere else so
  // it cannot be closed.
  imc_handle.fd = pair_[0];
  imc_handle.auto_close = true;

  // We use pid as process handle on Posix
  nacl_process_handle = handle();

#endif

  // Get the pid of the NaCl process
  base::ProcessId nacl_process_id = base::GetProcId(handle());

  ViewHostMsg_LaunchNaCl::WriteReplyParams(
      reply_msg_, imc_handle, nacl_process_handle, nacl_process_id);
  resource_message_filter_->Send(reply_msg_);
  resource_message_filter_ = NULL;
  reply_msg_ = NULL;

  SendStartMessage();
}

void NaClProcessHost::SendStartMessage() {
  nacl::FileDescriptor channel;
#if defined(OS_WIN)
  if (!DuplicateHandle(GetCurrentProcess(),
                       reinterpret_cast<HANDLE>(pair_[1]),
                       handle(),
                       reinterpret_cast<HANDLE*>(&channel),
                       GENERIC_READ | GENERIC_WRITE,
                       FALSE, DUPLICATE_CLOSE_SOURCE)) {
    return;
  }
#else
  channel.fd = dup(pair_[1]);
  channel.auto_close = true;
#endif
  Send(new NaClProcessMsg_Start(descriptor_, channel));
}

void NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED() << "Invalid message with type = " << msg.type();
}

URLRequestContext* NaClProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}
