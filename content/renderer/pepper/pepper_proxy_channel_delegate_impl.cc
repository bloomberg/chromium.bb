// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"

#include "content/child/child_process.h"
#include "content/common/sandbox_util.h"

namespace content {

PepperProxyChannelDelegateImpl::~PepperProxyChannelDelegateImpl() {}

base::SingleThreadTaskRunner*
PepperProxyChannelDelegateImpl::GetIPCTaskRunner() {
  // This is called only in the renderer so we know we have a child process.
  DCHECK(ChildProcess::current()) << "Must be in the renderer.";
  return ChildProcess::current()->io_task_runner();
}

base::WaitableEvent* PepperProxyChannelDelegateImpl::GetShutdownEvent() {
  DCHECK(ChildProcess::current()) << "Must be in the renderer.";
  return ChildProcess::current()->GetShutDownEvent();
}

IPC::PlatformFileForTransit
PepperProxyChannelDelegateImpl::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId remote_pid,
    bool should_close_source) {
  return BrokerGetFileHandleForProcess(handle, remote_pid, should_close_source);
}

base::SharedMemoryHandle
PepperProxyChannelDelegateImpl::ShareSharedMemoryHandleWithRemote(
    const base::SharedMemoryHandle& handle,
    base::ProcessId remote_pid) {
  base::PlatformFile local_platform_file =
#if defined(OS_POSIX)
      handle.fd;
#elif defined(OS_WIN)
      handle;
#else
#error Not implemented.
#endif
  return ShareHandleWithRemote(local_platform_file, remote_pid, false);
}

}  // namespace content
