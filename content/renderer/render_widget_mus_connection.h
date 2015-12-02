// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_MUS_CONNECTION_H_
#define CONTENT_RENDERER_RENDER_WIDGET_MUS_CONNECTION_H_

#include "base/macros.h"
#include "cc/output/output_surface.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_surface.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class RenderWidgetMusConnection : public mus::WindowTreeDelegate,
                                  public mus::WindowObserver {
 public:
  explicit RenderWidgetMusConnection(int routing_id);
  ~RenderWidgetMusConnection() override;

  // Connect to a WindowTreeClient request.
  void Bind(mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request);

  // Create a cc output surface.
  scoped_ptr<cc::OutputSurface> CreateOutputSurface();

  // Get the connection from a routing_id, if the connection doesn't exist,
  // a new connection will be created.
  static RenderWidgetMusConnection* GetOrCreate(int routing_id);

 private:
  // WindowTreeDelegate implementation:
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;
  void OnEmbed(mus::Window* root) override;
  void OnUnembed() override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  const int routing_id_;
  mus::Window* root_;
  scoped_ptr<mus::WindowSurfaceBinding> window_surface_binding_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_MUS_CONNECTION_H_
