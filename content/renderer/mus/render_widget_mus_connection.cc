// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/render_widget_mus_connection.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "services/ui/public/cpp/window_compositor_frame_sink.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace content {

namespace {

typedef std::map<int, RenderWidgetMusConnection*> ConnectionMap;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;
}

void RenderWidgetMusConnection::Bind(
    mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request) {
  // TODO(sad): crbug.com/672913
}

std::unique_ptr<cc::CompositorFrameSink>
RenderWidgetMusConnection::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  // TODO(sad): crbug.com/672913
  return nullptr;
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

}  // namespace content
