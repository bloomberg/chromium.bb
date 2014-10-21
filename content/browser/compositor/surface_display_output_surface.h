// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_DISPLAY_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_DISPLAY_OUTPUT_SURFACE_H_

#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"

namespace cc {
class Display;
class SurfaceManager;
}

namespace content {
class OnscreenDisplayClient;

// This class is maps a compositor OutputSurface to the surface system's Display
// concept, allowing a compositor client to submit frames for a native root
// window or physical display.
class SurfaceDisplayOutputSurface : public cc::OutputSurface,
                                    public cc::SurfaceFactoryClient {
 public:
  // The underlying cc::Display and cc::SurfaceManager must outlive this class.
  SurfaceDisplayOutputSurface(
      cc::SurfaceManager* surface_manager,
      cc::SurfaceIdAllocator* allocator,
      const scoped_refptr<cc::ContextProvider>& context_provider);
  ~SurfaceDisplayOutputSurface() override;

  void set_display_client(OnscreenDisplayClient* display_client) {
    display_client_ = display_client;
  }
  cc::SurfaceFactory* factory() { return &factory_; }
  void ReceivedVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

 private:
  void SwapBuffersComplete();

  OnscreenDisplayClient* display_client_;
  cc::SurfaceManager* surface_manager_;
  cc::SurfaceFactory factory_;
  gfx::Size display_size_;
  cc::SurfaceId surface_id_;
  cc::SurfaceIdAllocator* allocator_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDisplayOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_DISPLAY_OUTPUT_SURFACE_H_
