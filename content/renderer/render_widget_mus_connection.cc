// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_mus_connection.h"

#include <map>

#include "base/lazy_instance.h"
#include "components/mus/public/cpp/context_provider.h"
#include "components/mus/public/cpp/output_surface.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"

namespace content {

namespace {

typedef std::map<int, RenderWidgetMusConnection*> ConnectionMap;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;
}

RenderWidgetMusConnection::RenderWidgetMusConnection(int routing_id)
    : routing_id_(routing_id), root_(nullptr) {
  DCHECK(routing_id);
}

RenderWidgetMusConnection::~RenderWidgetMusConnection() {}

void RenderWidgetMusConnection::Bind(
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) {
  DCHECK(!root_);
  mus::WindowTreeConnection::Create(
      this, request.Pass(),
      mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

scoped_ptr<cc::OutputSurface> RenderWidgetMusConnection::CreateOutputSurface() {
  DCHECK(!window_surface_binding_);
  mus::mojom::GpuPtr gpu_service;
  MojoShellConnection::Get()->GetApplication()->ConnectToService("mojo:mus",
                                                                 &gpu_service);
  mus::mojom::CommandBufferPtr cb;
  gpu_service->CreateOffscreenGLES2Context(GetProxy(&cb));
  scoped_refptr<cc::ContextProvider> context_provider(
      new mus::ContextProvider(cb.PassInterface().PassHandle()));
  scoped_ptr<cc::OutputSurface> output_surface(new mus::OutputSurface(
      context_provider, mus::WindowSurface::Create(&window_surface_binding_)));
  if (root_) {
    root_->AttachSurface(mus::mojom::SURFACE_TYPE_DEFAULT,
                         window_surface_binding_.Pass());
  }
  return output_surface.Pass();
}

// static
RenderWidgetMusConnection* RenderWidgetMusConnection::GetOrCreate(
    int routing_id) {
  auto it = g_connections.Get().find(routing_id);
  if (it != g_connections.Get().end())
    return it->second;

  RenderWidgetMusConnection* connection =
      new RenderWidgetMusConnection(routing_id);
  g_connections.Get().insert(std::make_pair(routing_id, connection));
  return connection;
}

void RenderWidgetMusConnection::OnConnectionLost(
    mus::WindowTreeConnection* connection) {
  g_connections.Get().erase(routing_id_);
  delete this;
}

void RenderWidgetMusConnection::OnEmbed(mus::Window* root) {
  root_ = root;
  root_->AddObserver(this);
  if (window_surface_binding_) {
    root->AttachSurface(mus::mojom::SURFACE_TYPE_DEFAULT,
                        window_surface_binding_.Pass());
  }
}

void RenderWidgetMusConnection::OnUnembed() {}

void RenderWidgetMusConnection::OnWindowBoundsChanged(
    mus::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
}

}  // namespace content
