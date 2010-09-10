// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host_ui_shim.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/gpu_process_host.h"
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
  return Singleton<GpuProcessHostUIShim>::get();
}

int32 GpuProcessHostUIShim::NewRenderWidgetHostView(
    GpuNativeWindowHandle parent) {
  int32 routing_id = GetNextRoutingId();
  Send(new GpuMsg_NewRenderWidgetHostView(parent, routing_id));
  return routing_id;
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  ChromeThread::PostTask(ChromeThread::IO,
                         FROM_HERE,
                         new SendOnIOThreadTask(msg));
  return true;
}

int32 GpuProcessHostUIShim::GetNextRoutingId() {
  return ++last_routing_id_;
}

void GpuProcessHostUIShim::AddRoute(int32 routing_id,
                                    IPC::Channel::Listener* listener) {
  router_.AddRoute(routing_id, listener);
}

void GpuProcessHostUIShim::RemoveRoute(int32 routing_id) {
  router_.RemoveRoute(routing_id);
}

void GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  router_.RouteMessage(message);
}

void GpuProcessHostUIShim::CollectGraphicsInfoAsynchronously() {
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::IO));
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      new SendOnIOThreadTask(new GpuMsg_CollectGraphicsInfo()));
}
