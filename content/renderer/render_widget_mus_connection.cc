// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_mus_connection.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/mus/public/cpp/context_provider.h"
#include "components/mus/public/cpp/output_surface.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/renderer/compositor_mus_connection.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"

namespace content {

namespace {

typedef std::map<int, RenderWidgetMusConnection*> ConnectionMap;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;
}

void RenderWidgetMusConnection::Bind(
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  compositor_mus_connection_ = new CompositorMusConnection(
      routing_id_, render_thread->GetCompositorMainThreadTaskRunner(),
      render_thread->compositor_task_runner(), std::move(request),
      render_thread->input_handler_manager());
  if (window_surface_binding_) {
    compositor_mus_connection_->AttachSurfaceOnMainThread(
        std::move(window_surface_binding_));
  }
}

scoped_ptr<cc::OutputSurface> RenderWidgetMusConnection::CreateOutputSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!window_surface_binding_);
  mus::mojom::GpuPtr gpu_service;
  MojoShellConnection::Get()->GetApplication()->ConnectToService("mojo:mus",
                                                                 &gpu_service);
  mus::mojom::CommandBufferPtr cb;
  gpu_service->CreateOffscreenGLES2Context(GetProxy(&cb));
  scoped_refptr<cc::ContextProvider> context_provider(
      new mus::ContextProvider(cb.PassInterface().PassHandle()));
  scoped_ptr<cc::OutputSurface> surface(new mus::OutputSurface(
      context_provider, mus::WindowSurface::Create(&window_surface_binding_)));
  if (compositor_mus_connection_) {
    compositor_mus_connection_->AttachSurfaceOnMainThread(
        std::move(window_surface_binding_));
  }
  return surface;
}

// static
RenderWidgetMusConnection* RenderWidgetMusConnection::Get(int routing_id) {
  auto it = g_connections.Get().find(routing_id);
  if (it != g_connections.Get().end())
    return it->second;
  return nullptr;
}

// static
RenderWidgetMusConnection* RenderWidgetMusConnection::GetOrCreate(
    int routing_id) {
  RenderWidgetMusConnection* connection = Get(routing_id);
  if (!connection) {
    connection = new RenderWidgetMusConnection(routing_id);
    g_connections.Get().insert(std::make_pair(routing_id, connection));
  }
  return connection;
}

RenderWidgetMusConnection::RenderWidgetMusConnection(int routing_id)
    : routing_id_(routing_id) {
  DCHECK(routing_id);
}

RenderWidgetMusConnection::~RenderWidgetMusConnection() {}

void RenderWidgetMusConnection::OnConnectionLost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_connections.Get().erase(routing_id_);
  delete this;
}

void RenderWidgetMusConnection::OnWindowInputEvent(
    scoped_ptr<blink::WebInputEvent> input_event,
    const base::Closure& ack) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(fsamuel): Implement this once the following is complete:
  // 1. The Mus client lib supports manual event ACKing.
  // 2. Mus supports event coalescing.
  // 3. RenderWidget is refactored so that we don't send ACKs to the browser
  //    process.
  ack.Run();
}

}  // namespace content
