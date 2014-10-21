// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_CA_LAYER_TREE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_CA_LAYER_TREE_MAC_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "content/browser/compositor/browser_compositor_view_mac.h"

#if defined(__OBJC__)
#include <Cocoa/Cocoa.h>
#include "base/mac/scoped_nsobject.h"
#include "content/browser/compositor/io_surface_layer_mac.h"
#include "content/browser/compositor/software_layer_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#endif  // __OBJC__

namespace content {

#if defined(__OBJC__)

// BrowserCompositorCALayerTreeMac owns tree of CALayer and a ui::Compositor
// that is used to draw the layers. The CALayer tree can be attached to the
// NSView of a BrowserCompositorViewMac
class BrowserCompositorCALayerTreeMac
    : public IOSurfaceLayerClient {
 public:
  BrowserCompositorCALayerTreeMac();
  virtual ~BrowserCompositorCALayerTreeMac();
  static BrowserCompositorCALayerTreeMac* FromAcceleratedWidget(
      gfx::AcceleratedWidget widget);

  void SetView(BrowserCompositorViewMac* view);
  void ResetView();

  ui::Compositor* compositor() const { return compositor_.get(); }

  // Return true if the last frame swapped has a size in DIP of |dip_size|.
  bool HasFrameOfSize(const gfx::Size& dip_size) const;

  // Return the CGL renderer ID for the surface, if one is available.
  int GetRendererID() const;

  // Return true if the renderer should not be throttled by GPU back-pressure.
  bool IsRendererThrottlingDisabled() const;

  // Mark a bracket in which new frames are being pumped in a restricted nested
  // run loop.
  void BeginPumpingFrames();
  void EndPumpingFrames();

  void GotAcceleratedFrame(
      uint64 surface_handle, int output_surface_id,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::Size pixel_size, float scale_factor);

  void GotSoftwareFrame(
      cc::SoftwareFrameData* frame_data, float scale_factor, SkCanvas* canvas);

private:
  // IOSurfaceLayerClient implementation:
 bool IOSurfaceLayerShouldAckImmediately() const override;
 void IOSurfaceLayerDidDrawFrame() override;
 void IOSurfaceLayerHitError() override;

  void GotAcceleratedCAContextFrame(
      CAContextID ca_context_id, gfx::Size pixel_size, float scale_factor);

  void GotAcceleratedIOSurfaceFrame(
      IOSurfaceID io_surface_id, gfx::Size pixel_size, float scale_factor);

  // Remove a layer from the heirarchy and destroy it. Because the accelerated
  // layer types may be replaced by a layer of the same type, the layer to
  // destroy is parameterized, and, if it is the current layer, the current
  // layer is reset.
  void DestroyCAContextLayer(
      base::scoped_nsobject<CALayerHost> ca_context_layer);
  void DestroyIOSurfaceLayer(
      base::scoped_nsobject<IOSurfaceLayer> io_surface_layer);
  void DestroySoftwareLayer();

  // The BrowserCompositorViewMac that is using this as its internals.
  BrowserCompositorViewMac* view_;

  // A phony NSView handle used to identify this.
  gfx::AcceleratedWidget native_widget_;

  // The compositor drawing the contents of this view.
  scoped_ptr<ui::Compositor> compositor_;

  // A flipped layer, which acts as the parent of the compositing and software
  // layers. This layer is flipped so that the we don't need to recompute the
  // origin for sub-layers when their position changes (this is impossible when
  // using remote layers, as their size change cannot be synchronized with the
  // window). This indirection is needed because flipping hosted layers (like
  // |background_layer_| of RenderWidgetHostViewCocoa) leads to unpredictable
  // behavior.
  base::scoped_nsobject<CALayer> flipped_layer_;

  // The accelerated CoreAnimation layer hosted by the GPU process.
  base::scoped_nsobject<CALayerHost> ca_context_layer_;

  // The locally drawn accelerated CoreAnimation layer.
  base::scoped_nsobject<IOSurfaceLayer> io_surface_layer_;

  // The locally drawn software layer.
  base::scoped_nsobject<SoftwareLayer> software_layer_;

  // The output surface and latency info of the last accelerated surface that
  // was swapped. Sent back to the renderer when the accelerated surface is
  // drawn.
  int accelerated_output_surface_id_;
  std::vector<ui::LatencyInfo> accelerated_latency_info_;

  // The size in DIP of the last swap received from |compositor_|.
  gfx::Size last_swap_size_dip_;
};

#endif  // __OBJC__

void BrowserCompositorCALayerTreeMacGotAcceleratedFrame(
    gfx::AcceleratedWidget widget,
    uint64 surface_handle, int surface_id,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor,
    bool* disable_throttling, int* renderer_id);

void BrowserCompositorCALayerTreeMacGotSoftwareFrame(
    gfx::AcceleratedWidget widget,
    cc::SoftwareFrameData* frame_data, float scale_factor, SkCanvas* canvas);

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_CA_LAYER_TREE_MAC_H_
