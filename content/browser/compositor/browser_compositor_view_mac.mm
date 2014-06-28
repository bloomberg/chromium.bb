// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_mac.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "content/browser/compositor/browser_compositor_view_private_mac.h"

////////////////////////////////////////////////////////////////////////////////
// NSView (BrowserCompositorView)

@implementation NSView (BrowserCompositorView)

- (void)gotAcceleratedIOSurfaceFrame:(IOSurfaceID)surface_handle
                 withOutputSurfaceID:(int)surface_id
                     withLatencyInfo:(std::vector<ui::LatencyInfo>) latency_info
                       withPixelSize:(gfx::Size)pixel_size
                     withScaleFactor:(float)scale_factor {
  // The default implementation of additions to the NSView interface for browser
  // compositing should never be called. Log an error if they are.
  DLOG(ERROR) << "-[NSView gotAcceleratedIOSurfaceFrame] called on "
              << "non-overriden class.";
}

- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas {
  DLOG(ERROR) << "-[NSView gotSoftwareFrame] called on non-overridden class.";
}

@end  // NSView (BrowserCompositorView)

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewMac

namespace content {
namespace {

// The number of invalid BrowserCompositorViewMac objects. If this reaches
// zero, then the BrowserCompositorViewCocoa being held on to for recycling,
// |g_recyclable_cocoa_view|, will be freed.
uint32 g_invalid_view_count = 0;

// A spare BrowserCompositorViewCocoa kept around for recycling.
base::LazyInstance<base::scoped_nsobject<BrowserCompositorViewCocoa>>
  g_recyclable_cocoa_view;

}  // namespace

BrowserCompositorViewMac* BrowserCompositorViewMac::CreateForClient(
    BrowserCompositorViewMacClient* client) {
  return new BrowserCompositorViewMac(client);
}

BrowserCompositorViewMac* BrowserCompositorViewMac::CreateInvalid() {
  return new BrowserCompositorViewMac(NULL);
}

BrowserCompositorViewMac::BrowserCompositorViewMac(
    BrowserCompositorViewMacClient* client) : client_(client) {
  if (client_) {
    // Try to use the recyclable BrowserCompositorViewMac if there is one,
    // otherwise allocate a new one.
    // TODO(ccameron): If there exists a frame in flight (swap has been called
    // by the compositor, but the frame has not arrived from the GPU process
    // yet), then that frame may inappropriately flash in the new view.
    swap(g_recyclable_cocoa_view.Get(), cocoa_view_);
    if (!cocoa_view_)
      cocoa_view_.reset([[BrowserCompositorViewCocoa alloc] init]);
    [cocoa_view_ setClient:client_];
  } else {
    g_invalid_view_count += 1;
  }
}

BrowserCompositorViewMac::~BrowserCompositorViewMac() {
  if (client_) {
    [cocoa_view_ setClient:NULL];
    // If there are invalid views that may want need a
    // BrowserCompositorViewCocoa momentarily, then allow the
    // BrowserCompositorViewCocoa to be recycled. Otherwise, discard it.
    if (g_invalid_view_count)
      swap(g_recyclable_cocoa_view.Get(), cocoa_view_);
  } else {
    // If this is the last invalid view going away, then discard the
    // BrowserCompositorViewCocoa that was being saved to recycle.
    DCHECK_GT(g_invalid_view_count, 0u);
    g_invalid_view_count -= 1;
    if (!g_invalid_view_count)
      g_recyclable_cocoa_view.Get().reset();
  }
}

ui::Compositor* BrowserCompositorViewMac::GetCompositor() const {
  DCHECK(cocoa_view_);
  return [cocoa_view_ compositor];
}

}  // namespace content
