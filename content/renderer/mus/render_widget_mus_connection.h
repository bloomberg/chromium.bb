// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_RENDER_WIDGET_MUS_CONNECTION_H_
#define CONTENT_RENDERER_MUS_RENDER_WIDGET_MUS_CONNECTION_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/context_provider.h"
#include "content/common/content_export.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace ui {
class WindowCompositorFrameSinkBinding;
}

namespace content {

// This lives in the main-thread, and manages the connection to the mus window
// server for a RenderWidget.
class CONTENT_EXPORT RenderWidgetMusConnection {
 public:
  // Bind to a WindowTreeClient request.
  void Bind(mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request);

  // Create a cc output surface.
  std::unique_ptr<cc::CompositorFrameSink> CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  static RenderWidgetMusConnection* Get(int routing_id);

  // Get the connection from a routing_id, if the connection doesn't exist,
  // a new connection will be created.
  static RenderWidgetMusConnection* GetOrCreate(int routing_id);

 private:
  friend class CompositorMusConnection;
  friend class CompositorMusConnectionTest;

  explicit RenderWidgetMusConnection(int routing_id);
  ~RenderWidgetMusConnection();

  const int routing_id_;
  std::unique_ptr<ui::WindowCompositorFrameSinkBinding>
      window_compositor_frame_sink_binding_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_RENDER_WIDGET_MUS_CONNECTION_H_
