// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_ACCELERATED_PLUGIN_VIEW_MAC_H
#define CHROME_BROWSER_RENDERER_HOST_ACCELERATED_PLUGIN_VIEW_MAC_H

#import <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>

#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/native_widget_types.h"

class RenderWidgetHostViewMac;

// Informal protocol implemented by windows that need to be informed explicitly
// about underlay surfaces.
@interface NSObject (UnderlayableSurface)
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;
@end

// This subclass of NSView hosts the output of accelerated plugins on
// the page.

// This class takes a couple of locks, some indirectly. The lock hiearchy is:
// 1. The DisplayLink lock, implicit to the display link owned by this view.
//    It is taken by the framework before |DrawOneAcceleratedPluginCallback()|
//    is called, and during CVDisplayLink* function calls.
// 2. The CGL lock, taken explicitly.
// 3. The AcceleratedSurfaceContainerManagerMac's lock, which it takes when any
//    of its methods are called.
//
// No code should ever try to acquire a lock further up in the hierarchy if it
// already owns a lower lock. For example, while the CGL lock is taken, no
// CVDisplayLink* functions must be called.
@interface AcceleratedPluginView : NSView {
  scoped_nsobject<NSOpenGLPixelFormat> glPixelFormat_;
  CGLPixelFormatObj cglPixelFormat_;  // weak, backed by |glPixelFormat_|.
  scoped_nsobject<NSOpenGLContext> glContext_;
  CGLContextObj cglContext_;  // weak, backed by |glContext_|.

  CVDisplayLinkRef displayLink_;  // Owned by us.

  RenderWidgetHostViewMac* renderWidgetHostView_;  // weak
  gfx::PluginWindowHandle pluginHandle_;  // weak

  // The number of swap buffers calls that have been requested by the
  // GPU process, or a monotonically increasing number of calls to
  // updateSwapBuffersCount:fromRenderer:routeId: if the update came
  // from an accelerated plugin.
  uint64 swapBuffersCount_;
  // The number of swap buffers calls that have been processed by the
  // display link thread. This is only used with the GPU process
  // update path.
  volatile uint64 acknowledgedSwapBuffersCount_;

  // Auxiliary information needed to formulate an acknowledgment to
  // the GPU process. These are constant after the first message.
  // These are all zero for updates coming from a plugin process.
  volatile int rendererId_;
  volatile int32 routeId_;
  volatile int gpuHostId_;

  // Cocoa methods can only be called on the main thread, so have a copy of the
  // view's size, since it's required on the displaylink thread.
  NSSize cachedSize_;

  // Rects that should show web content rather than plugin content.
  scoped_nsobject<NSArray> cutoutRects_;

  // -globalFrameDidChange: can be called recursively, this counts how often it
  // holds the CGL lock.
  int globalFrameDidChangeCGLLockCount_;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r
                         pluginHandle:(gfx::PluginWindowHandle)pluginHandle;
- (void)drawView;

// Sets the list of rectangles that should show the web page, rather than the
// accelerated plugin. This is used to simulate the iframe-based trick that web
// pages have long used to show web content above windowed plugins on Windows
// and Linux.
- (void)setCutoutRects:(NSArray*)cutout_rects;

// Updates the number of swap buffers calls that have been requested.
// This is currently called with non-zero values only in response to
// updates from the GPU process. For accelerated plugins, all zeros
// are passed, and the view takes this as a hint that no flow control
// or acknowledgment of the swap buffers are desired.
- (void)updateSwapBuffersCount:(uint64)count
                  fromRenderer:(int)rendererId
                       routeId:(int32)routeId
                     gpuHostId:(int)gpuHostId;

// NSViews autorelease subviews when they die. The RWHVMac gets destroyed when
// RHWVCocoa gets dealloc'd, which means the AcceleratedPluginView child views
// can be around a little longer than the RWHVMac. This is called when the
// RWHVMac is about to be deleted (but it's still valid while this method runs).
- (void)onRenderWidgetHostViewGone;

// This _must_ be atomic, since it's accessed from several threads.
@property NSSize cachedSize;
@end

#endif  // CHROME_BROWSER_RENDERER_HOST_ACCELERATED_PLUGIN_VIEW_MAC_H
