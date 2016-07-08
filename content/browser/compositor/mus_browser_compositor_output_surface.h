// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_MUS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_MUS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/ui/public/cpp/window_surface_client.h"

namespace ui {
class Window;
class WindowSurface;
}

namespace content {

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class MusBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface,
      public ui::WindowSurfaceClient {
 public:
  MusBrowserCompositorOutputSurface(
      gpu::SurfaceHandle surface_handle,
      scoped_refptr<ContextProviderCommandBuffer> context,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  ~MusBrowserCompositorOutputSurface() override;

 protected:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

  // ui::WindowSurfaceClient:
  void OnResourcesReturned(
      ui::WindowSurface* surface,
      mojo::Array<cc::ReturnedResource> resources) override;

 private:
  uint32_t AllocateResourceId();
  void FreeResourceId(uint32_t id);
  const gpu::Mailbox& GetMailboxFromResourceId(uint32_t id);

  ui::Window* ui_window_;
  std::unique_ptr<ui::WindowSurface> ui_window_surface_;
  std::vector<uint32_t> free_resource_ids_;
  std::vector<gpu::Mailbox> mailboxes_;

  DISALLOW_COPY_AND_ASSIGN(MusBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_MUS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
