// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host_ui_shim.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/gpu_messages.h"

// Tasks used by this file
namespace {

class SendOnIOThreadTask : public Task {
 public:
  explicit SendOnIOThreadTask(IPC::Message* msg) : msg_(msg) {
  }

 private:
  void Run() {
    GpuProcessHost::Get()->Send(msg_);
  }
  IPC::Message* msg_;
};

}  // namespace

GpuProcessHostUIShim::GpuProcessHostUIShim() : last_routing_id_(1) {
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<GpuProcessHostUIShim>::get();
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          new SendOnIOThreadTask(msg));
  return true;
}

int32 GpuProcessHostUIShim::GetNextRoutingId() {
  DCHECK(CalledOnValidThread());
  return ++last_routing_id_;
}

void GpuProcessHostUIShim::AddRoute(int32 routing_id,
                                    IPC::Channel::Listener* listener) {
  DCHECK(CalledOnValidThread());
  router_.AddRoute(routing_id, listener);
}

void GpuProcessHostUIShim::RemoveRoute(int32 routing_id) {
  DCHECK(CalledOnValidThread());
  router_.RemoveRoute(routing_id);
}

void GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  if (message.routing_id() == MSG_ROUTING_CONTROL)
    OnControlMessageReceived(message);
  else
    router_.RouteMessage(message);
}

void GpuProcessHostUIShim::CollectGraphicsInfoAsynchronously() {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      new SendOnIOThreadTask(new GpuMsg_CollectGraphicsInfo()));
}

void GpuProcessHostUIShim::SendAboutGpuCrash() {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      new SendOnIOThreadTask(new GpuMsg_Crash()));
}

void GpuProcessHostUIShim::SendAboutGpuHang() {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      new SendOnIOThreadTask(new GpuMsg_Hang()));
}

const GPUInfo& GpuProcessHostUIShim::gpu_info() const {
  DCHECK(CalledOnValidThread());
  return gpu_info_;
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(const GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
  child_process_logging::SetGpuInfo(gpu_info);
}

void GpuProcessHostUIShim::OnScheduleComposite(int renderer_id,
    int render_view_id) {
  RenderViewHost* host = RenderViewHost::FromID(renderer_id,
                                                render_view_id);
  if (!host) {
    return;
  }
  host->ScheduleComposite();
}
void GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ScheduleComposite,
                        OnScheduleComposite);
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}
