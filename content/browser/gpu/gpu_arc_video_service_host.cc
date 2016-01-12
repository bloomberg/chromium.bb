// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_arc_video_service_host.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/arc_video_host_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace content {

namespace {

void CreateChannelOnIOThread(
    const GpuProcessHost::CreateArcVideoAcceleratorChannelCallback& callback) {
  GpuProcessHost* gpu_process_host =
      GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                          CAUSE_FOR_GPU_LAUNCH_ARCVIDEOACCELERATOR);
  gpu_process_host->CreateArcVideoAcceleratorChannel(callback);
}

void HandleChannelCreatedReply(
    const arc::VideoHost::OnRequestArcVideoAcceleratorChannelCallback& callback,
    const IPC::ChannelHandle& handle) {
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::embedder::CreatePlatformHandleWrapper(
      mojo::embedder::ScopedPlatformHandle(
          mojo::embedder::PlatformHandle(handle.socket.fd)),
      &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    callback.Run(mojo::ScopedHandle());
    return;
  }
  callback.Run(mojo::ScopedHandle(mojo::Handle(wrapped_handle)));
}

}  // namespace

scoped_ptr<arc::VideoHostDelegate> CreateArcVideoHostDelegate() {
  return make_scoped_ptr(new GpuArcVideoServiceHost());
}

GpuArcVideoServiceHost::GpuArcVideoServiceHost()
    : io_task_runner_(content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO)) {}

GpuArcVideoServiceHost::~GpuArcVideoServiceHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GpuArcVideoServiceHost::OnRequestArcVideoAcceleratorChannel(
    const OnRequestArcVideoAcceleratorChannelCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CreateChannelOnIOThread,
                            base::Bind(&HandleChannelCreatedReply, callback)));
}

void GpuArcVideoServiceHost::OnStopping() {
  GpuProcessHost::SendOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                           CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH,
                           new GpuMsg_ShutdownArcVideoService());
}

}  // namespace content
