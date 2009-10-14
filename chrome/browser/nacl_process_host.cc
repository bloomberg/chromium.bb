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
    ResourceDispatcherHost *resource_dispatcher_host)
    : ChildProcessHost(NACL_PROCESS, resource_dispatcher_host),
      resource_dispatcher_host_(resource_dispatcher_host) {
  // TODO(gregoryd): fix this to include the nexe name.
  set_name(L"nexe name should appear here");
}

bool NaClProcessHost::Launch(ResourceMessageFilter* renderer_msg_filter,
                             const int descriptor,
                             nacl::FileDescriptor* handle) {
#ifdef DISABLE_NACL
  NOTIMPLEMENTED() << "Native Client disabled at build time";
  return false;
#else
  nacl::Handle pair[2];
  bool success = false;
  // Create a connected socket
  if (nacl::SocketPair(pair) == -1) {
    NATIVE_HANDLE(*handle) = nacl::kInvalidHandle;
    return false;
  }

  // Launch the process
  success = LaunchSelLdr(renderer_msg_filter, descriptor, pair[1]);

  if (!success) {
    nacl::Close(pair[0]);
    NATIVE_HANDLE(*handle) = nacl::kInvalidHandle;
    return false;
  }

  nacl::Handle duplicate_handle = nacl::kInvalidHandle;
#if NACL_WINDOWS
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  reinterpret_cast<HANDLE>(pair[0]),
                  renderer_msg_filter->handle(),
                  reinterpret_cast<HANDLE*>(&duplicate_handle),
                  GENERIC_READ | GENERIC_WRITE,
                  FALSE,
                  DUPLICATE_CLOSE_SOURCE);
  *handle = duplicate_handle;
#else
  duplicate_handle = pair[0];
  int flags = fcntl(duplicate_handle, F_GETFD);
  if (flags != -1) {
    flags |= FD_CLOEXEC;
    fcntl(duplicate_handle, F_SETFD, flags);
  }
  // No need to dup the handle - we don't pass it anywhere else so
  // it cannot be closed.
  handle->fd = duplicate_handle;
  handle->auto_close = true;
#endif

  return true;
#endif  // DISABLE_NACL
}

bool NaClProcessHost::LaunchSelLdr(ResourceMessageFilter* renderer_msg_filter,
                                   const int descriptor,
                                   const nacl::Handle handle) {
  if (!CreateChannel())
    return false;

  // Build command line for nacl.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  std::wstring exe_path =
      browser_command_line.GetSwitchValue(switches::kBrowserSubprocessPath);
  if (exe_path.empty() && !PathService::Get(base::FILE_EXE, &exe_path))
    return false;

  CommandLine cmd_line(exe_path);
  if (logging::DialogsAreSuppressed())
    cmd_line.AppendSwitch(switches::kNoErrorDialogs);

  // propagate the following switches to the plugin command line (along with
  // any associated values) if present in the browser command line
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

  // send a message with duplicated handle to sel_ldr
  return SendStartMessage(process, descriptor, handle);
}

bool NaClProcessHost::SendStartMessage(base::ProcessHandle process,
                                       int descriptor,
                                       nacl::Handle handle) {
  nacl::FileDescriptor channel;
#if defined(OS_WIN)
  if (!DuplicateHandle(GetCurrentProcess(),
                       reinterpret_cast<HANDLE>(handle),
                       process,
                       reinterpret_cast<HANDLE*>(&channel),
                       GENERIC_READ | GENERIC_WRITE,
                       FALSE, DUPLICATE_CLOSE_SOURCE)) {
    return false;
  }
#else
  channel.fd = dup(handle);
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
