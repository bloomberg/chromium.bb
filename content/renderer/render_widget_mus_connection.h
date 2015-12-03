// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_MUS_CONNECTION_H_
#define CONTENT_RENDERER_RENDER_WIDGET_MUS_CONNECTION_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/output_surface.h"
#include "components/mus/public/cpp/window_surface.h"
#include "content/renderer/compositor_mus_connection.h"

namespace content {

class InputHandlerManager;

// Use on main thread.
class RenderWidgetMusConnection {
 public:
  // Bind to a WindowTreeClient request.
  void Bind(mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request);

  // Create a cc output surface.
  scoped_ptr<cc::OutputSurface> CreateOutputSurface();

  static RenderWidgetMusConnection* Get(int routing_id);

  // Get the connection from a routing_id, if the connection doesn't exist,
  // a new connection will be created.
  static RenderWidgetMusConnection* GetOrCreate(int routing_id);

 private:
  friend class CompositorMusConnection;

  explicit RenderWidgetMusConnection(int routing_id);
  ~RenderWidgetMusConnection();

  void OnConnectionLost();
  void OnWindowInputEvent(scoped_ptr<blink::WebInputEvent> input_event,
                          const base::Closure& ack);

  const int routing_id_;
  scoped_ptr<mus::WindowSurfaceBinding> window_surface_binding_;
  scoped_refptr<CompositorMusConnection> compositor_mus_connection_;

  // Used to verify single threaded access.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_MUS_CONNECTION_H_
