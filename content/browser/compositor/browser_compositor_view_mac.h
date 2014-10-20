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

class BrowserCompositorCALayerTreeMac;

// The interface through which BrowserCompositorViewMac calls back into
// RenderWidgetHostViewMac (or any other structure that wishes to draw a
// NSView backed by a ui::Compositor).
// TODO(ccameron): This interface should be in the ui namespace.
class BrowserCompositorViewMacClient {
 public:
  // Drawing is usually throttled by the rate at which CoreAnimation draws
  // frames to the screen. This can be used to disable throttling.
  virtual bool BrowserCompositorViewShouldAckImmediately() const = 0;

  // Called when a frame is drawn, and used to pass latency info back to the
  // renderer (if any).
  virtual void BrowserCompositorViewFrameSwapped(
      const std::vector<ui::LatencyInfo>& latency_info) = 0;
};

// The class to hold the ui::Compositor which is used to draw a NSView.
// TODO(ccameron): This should implement an interface in the ui namespace.
class BrowserCompositorViewMac {
 public:
  // This will install the NSView which is drawn by the ui::Compositor into
  // the NSView provided by the client. Note that |client|, |native_view|, and
  // |ui_root_layer| outlive their BrowserCompositorViewMac object.
  BrowserCompositorViewMac(
      BrowserCompositorViewMacClient* client,
      NSView* native_view,
      ui::Layer* ui_root_layer);
  ~BrowserCompositorViewMac();

  BrowserCompositorViewMacClient* client() const { return client_; }
  NSView* native_view() const { return native_view_; }
  ui::Layer* ui_root_layer() const { return ui_root_layer_; }

  // The ui::Compositor being used to render the NSView.
  // TODO(ccameron): This should be in the ui namespace interface.
  ui::Compositor* GetCompositor() const;

  // Return true if the last frame swapped has a size in DIP of |dip_size|.
  bool HasFrameOfSize(const gfx::Size& dip_size) const;

  // Mark a bracket in which new frames are pumped in a restricted nested run
  // loop because the the target window is resizing or because the view is being
  // shown after previously being hidden.
  void BeginPumpingFrames();
  void EndPumpingFrames();

 private:
  BrowserCompositorViewMacClient* client_;
  NSView* native_view_;
  ui::Layer* ui_root_layer_;

  // Because a ui::Compositor is expensive in terms of resources and
  // re-allocating a ui::Compositor is expensive in terms of work, this class
  // is largely used to manage recycled instances of
  // BrowserCompositorCALayerTreeMac, which actually has a ui::Compositor
  // instance and modifies the CALayers used to draw the NSView.
  scoped_ptr<BrowserCompositorCALayerTreeMac> ca_layer_tree_;
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
