// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/browser/renderer_host/gpu_message_filter.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/gpu_messages.h"

namespace content {

GpuMessageFilter::GpuMessageFilter(int render_process_id)
    : BrowserMessageFilter(GpuMsgStart),
      gpu_process_id_(0),
      render_process_id_(render_process_id),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GpuMessageFilter::~GpuMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool GpuMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_EstablishGpuChannel,
                                    OnEstablishGpuChannel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuMessageFilter::OnEstablishGpuChannel(
    CauseForGpuLaunch cause_for_gpu_launch,
    IPC::Message* reply_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_ptr<IPC::Message> reply(reply_ptr);

#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  // TODO(jbauman): Remove this when we know why renderer processes are
  // hanging on x86-64. https://crbug.com/577127
  if (!GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
    reply->set_reply_error();
    Send(reply.release());
    return;
  }
#endif

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch);
    if (!host) {
      reply->set_reply_error();
      Send(reply.release());
      return;
    }

    gpu_process_id_ = host->host_id();
  }

  bool preempts = false;
  bool allow_view_command_buffers = false;
  bool allow_real_time_streams = false;
  host->EstablishGpuChannel(
      render_process_id_,
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
          render_process_id_),
      preempts, allow_view_command_buffers, allow_real_time_streams,
      base::Bind(&GpuMessageFilter::EstablishChannelCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&reply)));
}

void GpuMessageFilter::EstablishChannelCallback(
    scoped_ptr<IPC::Message> reply,
    const IPC::ChannelHandle& channel,
    const gpu::GPUInfo& gpu_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GpuHostMsg_EstablishGpuChannel::WriteReplyParams(
      reply.get(), render_process_id_, channel, gpu_info);
  Send(reply.release());
}

}  // namespace content
