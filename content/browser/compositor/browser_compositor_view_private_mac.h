// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_PRIVATE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_PRIVATE_MAC_H_

#include "content/browser/compositor/browser_compositor_view_mac.h"

@class BrowserCompositorViewCocoa;

namespace content {

// BrowserCompositorViewCocoaClient is the interface through which
// gfx::NativeWidget (aka NSView aka BrowserCompositorViewCocoa) calls back to
// BrowserCompositorViewMacInternal.
class BrowserCompositorViewCocoaClient {
 public:
  virtual void GotAcceleratedIOSurfaceFrame(
      IOSurfaceID io_surface_id,
      int output_surface_id,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::Size pixel_size,
      float scale_factor) = 0;

  virtual void GotSoftwareFrame(
      cc::SoftwareFrameData* frame_data,
      float scale_factor,
      SkCanvas* canvas) = 0;
};

// BrowserCompositorViewMacInternal owns a NSView and a ui::Compositor that
// draws that view.
class BrowserCompositorViewMacInternal
    : public BrowserCompositorViewCocoaClient,
      public CompositingIOSurfaceLayerClient {
 public:
  BrowserCompositorViewMacInternal();
  virtual ~BrowserCompositorViewMacInternal();

  void SetClient(BrowserCompositorViewMacClient* client);
  void ResetClient();

  ui::Compositor* compositor() const { return compositor_.get(); }

 private:
  // BrowserCompositorViewCocoaClient implementation:
  virtual void GotAcceleratedIOSurfaceFrame(
      IOSurfaceID io_surface_id,
      int output_surface_id,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::Size pixel_size,
      float scale_factor) OVERRIDE;
  virtual void GotSoftwareFrame(
      cc::SoftwareFrameData* frame_data,
      float scale_factor,
      SkCanvas* canvas) OVERRIDE;

  // CompositingIOSurfaceLayerClient implementation:
  virtual void AcceleratedLayerDidDrawFrame(bool succeeded) OVERRIDE;

  // The client of the BrowserCompositorViewMac that is using this as its
  // internals.
  BrowserCompositorViewMacClient* client_;

  // The NSView drawn by the |compositor_|
  base::scoped_nsobject<BrowserCompositorViewCocoa> cocoa_view_;

  // The compositor drawing the contents of |cooca_view_|. Note that this must
  // be declared after |cocoa_view_|, so that it be destroyed first (because it
  // will reach into |cocoa_view_|).
  scoped_ptr<ui::Compositor> compositor_;


  // A flipped layer, which acts as the parent of the compositing and software
  // layers. This layer is flipped so that the we don't need to recompute the
  // origin for sub-layers when their position changes (this is impossible when
  // using remote layers, as their size change cannot be synchronized with the
  // window). This indirection is needed because flipping hosted layers (like
  // |background_layer_| of RenderWidgetHostViewCocoa) leads to unpredictable
  // behavior.
  base::scoped_nsobject<CALayer> flipped_layer_;

  base::scoped_nsobject<CompositingIOSurfaceLayer> accelerated_layer_;
  int accelerated_layer_output_surface_id_;
  std::vector<ui::LatencyInfo> accelerated_latency_info_;

  base::scoped_nsobject<SoftwareLayer> software_layer_;
};

}  // namespace content

// BrowserCompositorViewCocoa is the actual NSView to which the layers drawn
// by the ui::Compositor are attached.
@interface BrowserCompositorViewCocoa : NSView {
  content::BrowserCompositorViewCocoaClient* client_;
}

- (id)initWithClient:(content::BrowserCompositorViewCocoaClient*)client;

// Mark that the client provided at initialization is no longer valid and may
// not be called back into.
- (void)resetClient;
@end

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_PRIVATE_MAC_H_
