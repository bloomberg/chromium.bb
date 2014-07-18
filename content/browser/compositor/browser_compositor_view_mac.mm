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

// The number of placeholder objects allocated. If this reaches zero, then
// the BrowserCompositorViewCocoa being held on to for recycling,
// |g_recyclable_internal_view|, will be freed.
uint32 g_placeholder_count = 0;

// A spare BrowserCompositorViewCocoa kept around for recycling.
base::LazyInstance<scoped_ptr<BrowserCompositorViewMacInternal>>
  g_recyclable_internal_view;

}  // namespace

BrowserCompositorViewMac::BrowserCompositorViewMac(
    BrowserCompositorViewMacClient* client) : client_(client) {
  // Try to use the recyclable BrowserCompositorViewCocoa if there is one,
  // otherwise allocate a new one.
  // TODO(ccameron): If there exists a frame in flight (swap has been called
  // by the compositor, but the frame has not arrived from the GPU process
  // yet), then that frame may inappropriately flash in the new view.
  internal_view_ = g_recyclable_internal_view.Get().Pass();
  if (!internal_view_)
    internal_view_.reset(new BrowserCompositorViewMacInternal);
  internal_view_->SetClient(client_);
}

BrowserCompositorViewMac::~BrowserCompositorViewMac() {
  // Make this BrowserCompositorViewCocoa recyclable for future instances.
  internal_view_->ResetClient();
  g_recyclable_internal_view.Get() = internal_view_.Pass();

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorViewCocoa that we just populated.
  if (!g_placeholder_count)
    g_recyclable_internal_view.Get().reset();
}

ui::Compositor* BrowserCompositorViewMac::GetCompositor() const {
  DCHECK(internal_view_);
  return internal_view_->compositor();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewPlaceholderMac

BrowserCompositorViewPlaceholderMac::BrowserCompositorViewPlaceholderMac() {
  g_placeholder_count += 1;
}

BrowserCompositorViewPlaceholderMac::~BrowserCompositorViewPlaceholderMac() {
  DCHECK_GT(g_placeholder_count, 0u);
  g_placeholder_count -= 1;

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorViewCocoa.
  if (!g_placeholder_count)
    g_recyclable_internal_view.Get().reset();
}

}  // namespace content
