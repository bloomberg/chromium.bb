// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/nacl_process_host.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#endif

#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_messages.h"
#include "ipc/ipc_switches.h"

#if defined(OS_WIN)
#include "chrome/browser/sandbox_policy.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

NaClProcessHost::NaClProcessHost(
    ResourceDispatcherHost *resource_dispatcher_host,
    const std::wstring& url)
    : ChildProcessHost(NACL_PROCESS, resource_dispatcher_host),
      resource_dispatcher_host_(resource_dispatcher_host) {
  set_name(url);
}

bool NaClProcessHost::Launch(ResourceMessageFilter* renderer_msg_filter,
                             const int descriptor,
                             nacl::FileDescriptor* imc_handle,
                             base::ProcessHandle* nacl_process_handle,
                             base::ProcessId* nacl_process_id) {
#ifdef DISABLE_NACL
  NOTIMPLEMENTED() << "Native Client disabled at build time";
  return false;
#else
  nacl::Handle pair[2];
  bool success = false;

  NATIVE_HANDLE(*imc_handle) = nacl::kInvalidHandle;
  *nacl_process_handle = nacl::kInvalidHandle;
  *nacl_process_id = 0;

  // Create a connected socket
  if (nacl::SocketPair(pair) == -1) {
    return false;
  }

  // Launch the process
  success = LaunchSelLdr(renderer_msg_filter, descriptor, pair[1]);

  if (!success) {
    nacl::Close(pair[0]);
    return false;
  }

#if NACL_WINDOWS
  // Duplicate the IMC handle
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  reinterpret_cast<HANDLE>(pair[0]),
                  renderer_msg_filter->handle(),
                  imc_handle,
                  GENERIC_READ | GENERIC_WRITE,
                  FALSE,
                  DUPLICATE_CLOSE_SOURCE);

  // Duplicate the process handle
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  handle(),
                  renderer_msg_filter->handle(),
                  nacl_process_handle,
                  PROCESS_DUP_HANDLE,
                  FALSE,
                  0);

#else
  int flags = fcntl(pair[0], F_GETFD);
  if (flags != -1) {
    flags |= FD_CLOEXEC;
    fcntl(pair[0], F_SETFD, flags);
  }
  // No need to dup the imc_handle - we don't pass it anywhere else so
  // it cannot be closed.
  imc_handle->fd = pair[0];
  imc_handle->auto_close = true;

  // We use pid as process handle on Posix
  *nacl_process_handle = handle();

#endif

  // Get the pid of the NaCl process
  *nacl_process_id = base::GetProcId(handle());

  return true;
#endif  // DISABLE_NACL
}

bool NaClProcessHost::LaunchSelLdr(ResourceMessageFilter* renderer_msg_filter,
                                   const int descriptor,
                                   const nacl::Handle imc_handle) {
  if (!CreateChannel())
    return false;

  // Build command line for nacl.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  FilePath exe_path =
      browser_command_line.GetSwitchValuePath(switches::kBrowserSubprocessPath);
  if (exe_path.empty() && !PathService::Get(base::FILE_EXE, &exe_path))
    return false;

  CommandLine cmd_line(exe_path);
  if (logging::DialogsAreSuppressed())
    cmd_line.AppendSwitch(switches::kNoErrorDialogs);

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
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      cmd_line.AppendSwitchWithValue(
          switch_names[i],
          browser_command_line.GetSwitchValue(switch_names[i]));
    }
  }

  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kNaClProcess);

  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id()));

  base::ProcessHandle process = 0;
#if defined(OS_WIN)
  process = sandbox::StartProcess(&cmd_line);
#else
  base::file_handle_mapping_vector fds_to_map;
  const int ipcfd = channel().GetClientFileDescriptor();
  if (ipcfd > -1)
    fds_to_map.push_back(std::pair<int, int>(
        ipcfd, kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
  base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
#endif

  if (!process)
    return false;
  SetHandle(process);

  // send a message with duplicated imc_handle to sel_ldr
  return SendStartMessage(process, descriptor, imc_handle);
}

bool NaClProcessHost::SendStartMessage(base::ProcessHandle process,
                                       int descriptor,
                                       nacl::Handle imc_handle) {
  nacl::FileDescriptor channel;
#if defined(OS_WIN)
  if (!DuplicateHandle(GetCurrentProcess(),
                       reinterpret_cast<HANDLE>(imc_handle),
                       process,
                       reinterpret_cast<HANDLE*>(&channel),
                       GENERIC_READ | GENERIC_WRITE,
                       FALSE, DUPLICATE_CLOSE_SOURCE)) {
    return false;
  }
#else
  channel.fd = dup(imc_handle);
  channel.auto_close = true;
#endif
  NaClProcessMsg_Start* msg = new NaClProcessMsg_Start(descriptor,
                                                       channel);

  if (!Send(msg)) {
    return false;
  }
  return true;
}

void NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED() << "Invalid message with type = " << msg.type();
}

URLRequestContext* NaClProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}
