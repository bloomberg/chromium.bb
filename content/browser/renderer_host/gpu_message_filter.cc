// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/gpu_message_filter.h"

#include "base/callback.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/common/gpu_create_command_buffer_config.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"

GpuMessageFilter::GpuMessageFilter(int render_process_id)
    : gpu_host_id_(0),
      render_process_id_(render_process_id) {
}

// WeakPtrs to a GpuMessageFilter need to be Invalidated from
// the same thread from which they were created.
GpuMessageFilter::~GpuMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void GpuMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == GpuMsgStart)
    *thread = BrowserThread::UI;
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
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

// Callbacks used in this file.
namespace {

class EstablishChannelCallback
    : public CallbackRunner<Tuple2<const IPC::ChannelHandle&,
                                   const GPUInfo&> > {
 public:
  explicit EstablishChannelCallback(GpuMessageFilter* filter):
      filter_(filter->AsWeakPtr()) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  virtual void RunWithParams(const TupleType& params) {
    DispatchToMethod(this, &EstablishChannelCallback::Send, params);
  }

  void Send(const IPC::ChannelHandle& channel,
            const GPUInfo& gpu_info) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  virtual void RunWithParams(const TupleType& params) {
    DispatchToMethod(this, &SynchronizeCallback::Send, params);
  }

  void Send() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  virtual void RunWithParams(const TupleType& params) {
    DispatchToMethod(this, &CreateCommandBufferCallback::Send, params);
  }

  void Send(int32 route_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  scoped_ptr<EstablishChannelCallback> callback(
      new EstablishChannelCallback(this));

  // TODO(apatrick): Eventually, this will return the route ID of a
  // GpuProcessStub, from which the renderer process will create a
  // GpuProcessProxy. The renderer will use the proxy for all subsequent
  // communication with the GPU process. This means if the GPU process
  // terminates, the renderer process will not find itself unknowingly sending
  // IPCs to a newly launched GPU process. Also, I will rename this function
  // to something like OnCreateGpuProcess.
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(gpu_host_id_);
  if (!ui_shim) {
    ui_shim = GpuProcessHostUIShim::GetForRenderer(render_process_id_);
    if (!ui_shim) {
      callback->Run(IPC::ChannelHandle(), GPUInfo());
      return;
    }

    gpu_host_id_ = ui_shim->host_id();
  }

  ui_shim->EstablishGpuChannel(render_process_id_,
                               callback.release());
}

void GpuMessageFilter::OnSynchronizeGpu(IPC::Message* reply) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(gpu_host_id_);
  if (!ui_shim) {
    // TODO(apatrick): Eventually, this IPC message will be routed to a
    // GpuProcessStub with a particular routing ID. The error will be set if
    // the GpuProcessStub with that routing ID is not in the MessageRouter.
    reply->set_reply_error();
    Send(reply);
    return;
  }

  ui_shim->Synchronize(new SynchronizeCallback(this, reply));
}

void GpuMessageFilter::OnCreateViewCommandBuffer(
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(gpu_host_id_);
  if (!ui_shim) {
    // TODO(apatrick): Eventually, this IPC message will be routed to a
    // GpuProcessStub with a particular routing ID. The error will be set if
    // the GpuProcessStub with that routing ID is not in the MessageRouter.
    reply->set_reply_error();
    Send(reply);
    return;
  }

  ui_shim->CreateViewCommandBuffer(
      render_view_id,
      render_process_id_,
      init_params,
      new CreateCommandBufferCallback(this, reply));
}
