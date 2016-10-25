// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_MUS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_MUS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/surface_handle.h"

namespace ui {
class Window;
class WindowCompositorFrameSink;
}

namespace content {

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class MusBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface,
      public cc::CompositorFrameSinkClient {
 public:
  MusBrowserCompositorOutputSurface(
      ui::Window* window,
      scoped_refptr<ContextProviderCommandBuffer> context,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  ~MusBrowserCompositorOutputSurface() override;

 protected:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;

  // cc::CompositorFrameSinkClient:
  void SetBeginFrameSource(cc::BeginFrameSource* source) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void DidReceiveCompositorFrameAck() override;
  void DidLoseCompositorFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;

 private:
  uint32_t AllocateResourceId();
  void FreeResourceId(uint32_t id);
  const gpu::Mailbox& GetMailboxFromResourceId(uint32_t id);

  ui::Window* ui_window_;
  std::unique_ptr<ui::WindowCompositorFrameSink> ui_compositor_frame_sink_;
  std::vector<uint32_t> free_resource_ids_;
  std::vector<gpu::Mailbox> mailboxes_;

  DISALLOW_COPY_AND_ASSIGN(MusBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_MUS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
