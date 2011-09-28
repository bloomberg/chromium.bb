// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_PLUGIN_VIEW_MAC_H
#define CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_PLUGIN_VIEW_MAC_H

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
@interface AcceleratedPluginView : NSView {
  scoped_nsobject<NSOpenGLPixelFormat> glPixelFormat_;
  CGLPixelFormatObj cglPixelFormat_;  // weak, backed by |glPixelFormat_|.
  scoped_nsobject<NSOpenGLContext> glContext_;
  CGLContextObj cglContext_;  // weak, backed by |glContext_|.

  RenderWidgetHostViewMac* renderWidgetHostView_;  // weak
  gfx::PluginWindowHandle pluginHandle_;  // weak

  // Rects that should show web content rather than plugin content.
  scoped_nsobject<NSArray> cutoutRects_;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r
                         pluginHandle:(gfx::PluginWindowHandle)pluginHandle;

// Sets the list of rectangles that should show the web page, rather than the
// accelerated plugin. This is used to simulate the iframe-based trick that web
// pages have long used to show web content above windowed plugins on Windows
// and Linux.
- (void)setCutoutRects:(NSArray*)cutout_rects;

// NSViews autorelease subviews when they die. The RWHVMac gets destroyed when
// RHWVCocoa gets dealloc'd, which means the AcceleratedPluginView child views
// can be around a little longer than the RWHVMac. This is called when the
// RWHVMac is about to be deleted (but it's still valid while this method runs).
- (void)onRenderWidgetHostViewGone;

// Draw the view from the UI thread.
- (void)drawView;

@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_PLUGIN_VIEW_MAC_H
