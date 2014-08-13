// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_

#include <vector>

#include "cc/output/software_frame_data.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/compositor.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class BrowserCompositorViewMacInternal;

// The interface through which BrowserCompositorViewMac calls back into
// RenderWidgetHostViewMac (or any other structure that wishes to draw a
// NSView backed by a ui::Compositor).
class BrowserCompositorViewMacClient {
 public:
  // Drawing is usually throttled by the rate at which CoreAnimation draws
  // frames to the screen. This can be used to disable throttling.
  virtual bool BrowserCompositorViewShouldAckImmediately() const = 0;

  // Called when a frame is drawn, and used to pass latency info back to the
  // renderer (if any).
  virtual void BrowserCompositorViewFrameSwapped(
      const std::vector<ui::LatencyInfo>& latency_info) = 0;

  // Used to install the ui::Compositor-backed NSView as a child of its parent
  // view.
  virtual NSView* BrowserCompositorSuperview() = 0;

  // Used to install the root ui::Layer into the ui::Compositor.
  virtual ui::Layer* BrowserCompositorRootLayer() = 0;
};

// The class to hold a ui::Compositor-backed NSView. Because a ui::Compositor
// is expensive in terms of resources and re-allocating a ui::Compositor is
// expensive in terms of work, this class is largely used to manage recycled
// instances of BrowserCompositorViewCocoa, which actually is a NSView and
// has a ui::Compositor instance.
class BrowserCompositorViewMac {
 public:
  // This will install the NSView which is drawn by the ui::Compositor into
  // the NSView provided by the client.
  explicit BrowserCompositorViewMac(BrowserCompositorViewMacClient* client);
  ~BrowserCompositorViewMac();

  // The ui::Compositor being used to render the NSView.
  ui::Compositor* GetCompositor() const;

  // The client (used by the BrowserCompositorViewCocoa to access the client).
  BrowserCompositorViewMacClient* GetClient() const { return client_; }

  // Return true if the last frame swapped has a size in DIP of |dip_size|.
  bool HasFrameOfSize(const gfx::Size& dip_size) const;

  // Mark a bracket in which new frames are pumped in a restricted nested run
  // loop because the the target window is resizing or because the view is being
  // shown after previously being hidden.
  void BeginPumpingFrames();
  void EndPumpingFrames();

  static void GotAcceleratedFrame(
      gfx::AcceleratedWidget widget,
      uint64 surface_handle, int surface_id,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::Size pixel_size, float scale_factor,
      int gpu_host_id, int gpu_route_id);

  static void GotSoftwareFrame(
      gfx::AcceleratedWidget widget,
      cc::SoftwareFrameData* frame_data, float scale_factor, SkCanvas* canvas);

 private:
  BrowserCompositorViewMacClient* client_;
  scoped_ptr<BrowserCompositorViewMacInternal> internal_view_;
};

// A class to keep around whenever a BrowserCompositorViewMac may be created.
// While at least one instance of this class exists, a spare
// BrowserCompositorViewCocoa will be kept around to be recycled so that the
// next BrowserCompositorViewMac to be created will be be created quickly.
class BrowserCompositorViewPlaceholderMac {
 public:
  BrowserCompositorViewPlaceholderMac();
  ~BrowserCompositorViewPlaceholderMac();
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_
