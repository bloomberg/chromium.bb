// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gpu_message_filter.h"

#include "chrome/browser/gpu_process_host.h"
#include "chrome/common/gpu_create_command_buffer_config.h"
#include "chrome/common/gpu_messages.h"

GpuMessageFilter::GpuMessageFilter(
    int render_process_id)
    : render_process_id_(render_process_id) {
}

GpuMessageFilter::~GpuMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

bool GpuMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(GpuHostMsg_EstablishGpuChannel,
                        OnEstablishGpuChannel)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_SynchronizeGpu,
                                    OnSynchronizeGpu)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_CreateViewCommandBuffer,
                                    OnCreateViewCommandBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void GpuMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void GpuMessageFilter::OnEstablishGpuChannel() {
  GpuProcessHost::Get()->EstablishGpuChannel(render_process_id_, this);
}

void GpuMessageFilter::OnSynchronizeGpu(IPC::Message* reply) {
  // We handle this message (and the other GPU process messages) here
  // rather than handing the message to the GpuProcessHost for
  // dispatch so that we can use the DELAY_REPLY macro to synthesize
  // the reply message, and also send down a "this" pointer so that
  // the GPU process host can send the reply later.
  GpuProcessHost::Get()->Synchronize(reply, this);
}

void GpuMessageFilter::OnCreateViewCommandBuffer(
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply) {
  GpuProcessHost::Get()->CreateViewCommandBuffer(
      render_view_id, render_process_id_, init_params, reply, this);
}
