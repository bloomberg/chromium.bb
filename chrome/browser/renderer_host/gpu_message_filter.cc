// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gpu_message_filter.h"

#include "base/callback.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/common/gpu_create_command_buffer_config.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"

GpuMessageFilter::GpuMessageFilter(int render_process_id)
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

// Callbacks used in this file.
namespace {

class EstablishChannelCallback
    : public CallbackRunner<Tuple2<const IPC::ChannelHandle&,
                                   const GPUInfo&> > {
 public:
  explicit EstablishChannelCallback(GpuMessageFilter* filter):
    filter_(filter->AsWeakPtr()) {
  }

  virtual void RunWithParams(const TupleType& params) {
    DispatchToMethod(this, &EstablishChannelCallback::Send, params);
  }

  void Send(const IPC::ChannelHandle& channel,
            const GPUInfo& gpu_info) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ViewMsg_GpuChannelEstablished* reply =
        new ViewMsg_GpuChannelEstablished(channel, gpu_info);
    // If the renderer process is performing synchronous initialization,
    // it needs to handle this message before receiving the reply for
    // the synchronous GpuHostMsg_SynchronizeGpu message.
    reply->set_unblock(true);

    if (filter_)
      filter_->Send(reply);
  }

 private:
  base::WeakPtr<GpuMessageFilter> filter_;
};

class SynchronizeCallback : public CallbackRunner<Tuple0> {
 public:
  SynchronizeCallback(GpuMessageFilter* filter, IPC::Message* reply):
    filter_(filter->AsWeakPtr()),
    reply_(reply) {
  }

  virtual void RunWithParams(const TupleType& params) {
    DispatchToMethod(this, &SynchronizeCallback::Send, params);
  }

  void Send() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (filter_)
      filter_->Send(reply_);
  }

 private:
  base::WeakPtr<GpuMessageFilter> filter_;
  IPC::Message* reply_;
};

class CreateCommandBufferCallback : public CallbackRunner<Tuple1<int32> > {
 public:
  CreateCommandBufferCallback(GpuMessageFilter* filter,
                              IPC::Message* reply) :
    filter_(filter->AsWeakPtr()),
    reply_(reply) {
  }

  virtual void RunWithParams(const TupleType& params) {
    DispatchToMethod(this, &CreateCommandBufferCallback::Send, params);
  }

  void Send(int32 route_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(reply_, route_id);
    if (filter_)
      filter_->Send(reply_);
  }

 private:
  base::WeakPtr<GpuMessageFilter> filter_;
  IPC::Message* reply_;
};

}  // namespace

void GpuMessageFilter::OnEstablishGpuChannel() {
  GpuProcessHost::Get()->EstablishGpuChannel(
      render_process_id_, new EstablishChannelCallback(this));
}

void GpuMessageFilter::OnSynchronizeGpu(IPC::Message* reply) {
  GpuProcessHost::Get()->Synchronize(new SynchronizeCallback(this, reply));
}

void GpuMessageFilter::OnCreateViewCommandBuffer(
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply) {
  GpuProcessHost::Get()->CreateViewCommandBuffer(
      render_view_id, render_process_id_, init_params,
      new CreateCommandBufferCallback(this, reply));
}
