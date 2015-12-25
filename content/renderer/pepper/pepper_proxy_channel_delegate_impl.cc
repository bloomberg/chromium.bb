// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"

#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/sandbox_util.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "content/public/common/sandbox_init.h"
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

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
#if defined(OS_WIN) || defined(OS_MACOSX)
  base::SharedMemoryHandle duped_handle;
  bool success =
      BrokerDuplicateSharedMemoryHandle(handle, remote_pid, &duped_handle);
  if (success)
    return duped_handle;
  return base::SharedMemory::NULLHandle();
#else
  return base::SharedMemory::DuplicateHandle(handle);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

}  // namespace content
