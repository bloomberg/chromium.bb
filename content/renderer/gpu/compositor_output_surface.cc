// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_output_surface.h"

#include "base/message_loop_proxy.h"
#include "cc/compositor_frame.h"
#include "cc/output_surface_client.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

using cc::CompositorFrame;
using cc::SoftwareOutputDevice;
using WebKit::WebGraphicsContext3D;

namespace {
// There are several compositor surfaces in a process, but they share the same
// compositor thread, so we use a simple int here to track prefer-smoothness.
int g_prefer_smoothness_count = 0;
} // namespace

namespace content {

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
    cc::SoftwareOutputDevice* software_device)
    : output_surface_filter_(
          RenderThreadImpl::current()->compositor_output_surface_filter()),
      client_(NULL),
      routing_id_(routing_id),
      context3D_(context3D),
      software_device_(software_device),
      prefers_smoothness_(false),
      main_thread_id_(base::PlatformThread::CurrentId()) {
  DCHECK(output_surface_filter_);
  capabilities_.has_parent_compositor = false;
  DetachFromThread();
}

CompositorOutputSurface::~CompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  if (!client_)
    return;
  UpdateSmoothnessTakesPriority(false);
  output_surface_proxy_->ClearOutputSurface();
  output_surface_filter_->RemoveRoute(routing_id_);
}

const struct cc::OutputSurface::Capabilities&
    CompositorOutputSurface::Capabilities() const {
  DCHECK(CalledOnValidThread());
  return capabilities_;
}

bool CompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(CalledOnValidThread());
  DCHECK(!client_);
  if (context3D_.get()) {
    if (!context3D_->makeContextCurrent())
      return false;
  }

  client_ = client;

  output_surface_proxy_ = new CompositorOutputSurfaceProxy(this);
  output_surface_filter_->AddRoute(
      routing_id_,
      base::Bind(&CompositorOutputSurfaceProxy::OnMessageReceived,
                 output_surface_proxy_));

  return true;
}

WebGraphicsContext3D* CompositorOutputSurface::Context3D() const {
  DCHECK(CalledOnValidThread());
  return context3D_.get();
}

cc::SoftwareOutputDevice* CompositorOutputSurface::SoftwareDevice() const {
  return software_device_.get();
}

void CompositorOutputSurface::SendFrameToParentCompositor(
    cc::CompositorFrame* frame) {
  DCHECK(CalledOnValidThread());
  Send(new ViewHostMsg_SwapCompositorFrame(routing_id_, *frame));
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
    base::TimeTicks timebase, base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(client_);
  client_->OnVSyncParametersChanged(timebase, interval);
}

bool CompositorOutputSurface::Send(IPC::Message* message) {
  return ChildThread::current()->sync_message_filter()->Send(message);
}

namespace {
#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  void SetThreadsPriorityToIdle(base::PlatformThreadId id) {
    int nice_value = 10; // Idle priority.
    setpriority(PRIO_PROCESS, id, nice_value);
  }
  void SetThreadsPriorityToDefault(base::PlatformThreadId id) {
    int nice_value = 0; // Default priority.
    setpriority(PRIO_PROCESS, id, nice_value);
  }
#else
  void SetThreadsPriorityToIdle(base::PlatformThreadId id) {}
  void SetThreadsPriorityToDefault(base::PlatformThreadId id) {}
#endif
}

void CompositorOutputSurface::UpdateSmoothnessTakesPriority(
    bool prefers_smoothness) {
#if ENABLE_DCHECK
  // If we use different compositor threads, we need to
  // use an atomic int to track prefer smoothness count.
  static int g_last_thread = base::PlatformThread::CurrentId();
  DCHECK_EQ(g_last_thread, base::PlatformThread::CurrentId());
#endif
  if (prefers_smoothness_ == prefers_smoothness)
    return;
  // If this is the first surface to start preferring smoothness,
  // Throttle the main thread's priority.
  if (prefers_smoothness_ == false &&
      ++g_prefer_smoothness_count == 1) {
    SetThreadsPriorityToIdle(main_thread_id_);
  }
  // If this is the last surface to stop preferring smoothness,
  // Reset the main thread's priority to the default.
  if (prefers_smoothness_ == true &&
      --g_prefer_smoothness_count == 0) {
    SetThreadsPriorityToDefault(main_thread_id_);
  }
  prefers_smoothness_ = prefers_smoothness;
}

}  // namespace content
