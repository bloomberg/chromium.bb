// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "cc/output/software_frame_data.h"
#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"
#include "content/browser/renderer_host/software_layer_mac.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/compositor.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"

@class BrowserCompositorViewCocoa;

// Additions to the NSView interface for compositor frames.
@interface NSView (BrowserCompositorView)
- (void)gotAcceleratedIOSurfaceFrame:(IOSurfaceID)surface_handle
                 withOutputSurfaceID:(int)surface_id
                     withLatencyInfo:(std::vector<ui::LatencyInfo>) latency_info
                       withPixelSize:(gfx::Size)pixel_size
                     withScaleFactor:(float)scale_factor;

- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas;
@end  // NSView (BrowserCompositorView)


namespace content {

// The interface through which BrowserCompositorViewMac calls back into
// RenderWidgetHostViewMac (or any other structure that wishes to draw a
// NSView backed by a ui::Compositor).
class BrowserCompositorViewMacClient {
 public:
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
  static BrowserCompositorViewMac* CreateForClient(
      BrowserCompositorViewMacClient* client);

  // This is used as a placeholder to indicate that the owner may want a full
  // instance soon. One recycled instance of BrowserCompositorViewCocoa will
  // be kept around as long as a non-zero number of invalid instances are
  // present.
  static BrowserCompositorViewMac* CreateInvalid();

  ~BrowserCompositorViewMac();

  // Returns true if this was created with CreateForClient, as opposed to
  // CreateInvalid.
  bool IsValid() const { return client_; }

  // The ui::Compositor being used to render the NSView.
  ui::Compositor* GetCompositor() const;

  // The client (used by the BrowserCompositorViewCocoa to access the client).
  BrowserCompositorViewMacClient* GetClient() const { return client_; }

 private:
  BrowserCompositorViewMac(BrowserCompositorViewMacClient* client);

  BrowserCompositorViewMacClient* client_;
  base::scoped_nsobject<BrowserCompositorViewCocoa> cocoa_view_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_
