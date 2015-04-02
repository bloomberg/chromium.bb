// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;

namespace cc {
class CompositorFrame;
class CompositorFrameAck;
}

namespace gfx {
class Transform;
};

namespace gpu {
class GLInProcessContext;
}

namespace content {

class SynchronousCompositorClient;
class WebContents;

// Interface for embedders that wish to direct compositing operations
// synchronously under their own control. Only meaningful when the
// kEnableSyncrhonousRendererCompositor flag is specified.
class CONTENT_EXPORT SynchronousCompositor {
 public:
  // Must be called once per WebContents instance. Will create the compositor
  // instance as needed, but only if |client| is non-nullptr.
  static void SetClientForWebContents(WebContents* contents,
                                      SynchronousCompositorClient* client);

  static void SetGpuService(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service);

  // By default, synchronous compopsitor records the full layer, not only
  // what is inside and around the view port. This can be used to switch
  // between this record-full-layer behavior and normal record-around-viewport
  // behavior.
  static void SetRecordFullDocument(bool record_full_document);

  // Synchronously initialize compositor for hardware draw. Can only be called
  // while compositor is in software only mode, either after compositor is
  // first created or after ReleaseHwDraw is called. It is invalid to
  // DemandDrawHw before this returns true.
  virtual bool InitializeHwDraw() = 0;

  // Reverse of InitializeHwDraw above. Can only be called while hardware draw
  // is already initialized. Brings compositor back to software only mode and
  // releases all hardware resources.
  virtual void ReleaseHwDraw() = 0;

  // "On demand" hardware draw. The content is first clipped to |damage_area|,
  // then transformed through |transform|, and finally clipped to |view_size|.
  virtual scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      gfx::Size surface_size,
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      gfx::Rect viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) = 0;

  // For delegated rendering, return resources from parent compositor to this.
  // Note that all resources must be returned before ReleaseHwDraw.
  virtual void ReturnResources(const cc::CompositorFrameAck& frame_ack) = 0;

  // "On demand" SW draw, into the supplied canvas (observing the transform
  // and clip set there-in).
  virtual bool DemandDrawSw(SkCanvas* canvas) = 0;

  // Set the memory limit policy of this compositor.
  virtual void SetMemoryPolicy(size_t bytes_limit) = 0;

  // Should be called by the embedder after the embedder had modified the
  // scroll offset of the root layer (as returned by
  // SynchronousCompositorClient::GetTotalRootLayerScrollOffset).
  virtual void DidChangeRootLayerScrollOffset() = 0;

 protected:
  virtual ~SynchronousCompositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_
