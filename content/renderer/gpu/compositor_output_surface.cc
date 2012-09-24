// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_output_surface.h"

#include "base/message_loop_proxy.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorOutputSurfaceClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

using WebKit::WebGraphicsContext3D;
using WebKit::WebCompositorSoftwareOutputDevice;

//------------------------------------------------------------------------------

// static
IPC::ForwardingMessageFilter* CompositorOutputSurface::CreateFilter(
    base::TaskRunner* target_task_runner)
{
  uint32 messages_to_filter[] = {ViewMsg_UpdateVSyncParameters::ID};
  return new IPC::ForwardingMessageFilter(
      messages_to_filter, arraysize(messages_to_filter),
      target_task_runner);
}

CompositorOutputSurface::CompositorOutputSurface(
    int32 routing_id,
    WebGraphicsContext3D* context3D,
    WebCompositorSoftwareOutputDevice* software_device)
    : output_surface_filter_(
          RenderThreadImpl::current()->compositor_output_surface_filter()),
      client_(NULL),
      routing_id_(routing_id),
      context3D_(context3D),
      software_device_(software_device) {
  DCHECK(output_surface_filter_);
  capabilities_.hasParentCompositor = false;
  DetachFromThread();
}

CompositorOutputSurface::~CompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  output_surface_filter_->RemoveRoute(routing_id_);
}

const WebKit::WebCompositorOutputSurface::Capabilities&
    CompositorOutputSurface::capabilities() const {
  DCHECK(CalledOnValidThread());
  return capabilities_;
}

bool CompositorOutputSurface::bindToClient(
    WebKit::WebCompositorOutputSurfaceClient* client) {
  DCHECK(CalledOnValidThread());
  DCHECK(!client_);
  if (context3D_.get()) {
    if (!context3D_->makeContextCurrent())
      return false;
  }

  client_ = client;

  output_surface_filter_->AddRoute(
      routing_id_,
      base::Bind(&CompositorOutputSurface::OnMessageReceived,
                 base::Unretained(this)));

  return true;
}

WebGraphicsContext3D* CompositorOutputSurface::context3D() const {
  DCHECK(CalledOnValidThread());
  return context3D_.get();
}

WebCompositorSoftwareOutputDevice* CompositorOutputSurface::softwareDevice()
    const {
  return software_device_.get();
}

void CompositorOutputSurface::sendFrameToParentCompositor(
    const WebKit::WebCompositorFrame&) {
  DCHECK(CalledOnValidThread());
  NOTREACHED();
}

void CompositorOutputSurface::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  IPC_BEGIN_MESSAGE_MAP(CompositorOutputSurface, message)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateVSyncParameters, OnUpdateVSyncParameters);
  IPC_END_MESSAGE_MAP()
}

void CompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(client_);
  double monotonicTimebase = timebase.ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
  double intervalInSeconds = interval.ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
  client_->onVSyncParametersChanged(monotonicTimebase, intervalInSeconds);
}
