// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/accelerated_plugin_view_mac.h"

#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/browser_thread.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

using content::BrowserThread;

@interface AcceleratedPluginView(Private)

// Initialize the OpenGL context.
- (void)initOpenGLContext;

@end  // AcceleratedPluginView(Private)

@implementation AcceleratedPluginView

- (void)drawView {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser", "AcceleratedPluginViewMac::drawView");

  if (renderWidgetHostView_ && cglContext_) {
    // TODO(thakis): Pixel or view coordinates for size?
    renderWidgetHostView_->DrawAcceleratedSurfaceInstance(
        cglContext_, pluginHandle_, [super frame].size);
    CGLFlushDrawable(cglContext_);
  }

  CGLSetCurrentContext(0);
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r
                         pluginHandle:(gfx::PluginWindowHandle)pluginHandle {
  if ((self = [super initWithFrame:NSZeroRect])) {
    renderWidgetHostView_ = r;
    pluginHandle_ = pluginHandle;

    [self setAutoresizingMask:NSViewMaxXMargin|NSViewMinYMargin];

    handlingGlobalFrameDidChange_ = NO;
    [[NSNotificationCenter defaultCenter]
         addObserver:self
            selector:@selector(globalFrameDidChange:)
                name:NSViewGlobalFrameDidChangeNotification
              object:self];
  }
  return self;
}

- (void)dealloc {
  if (renderWidgetHostView_)
    renderWidgetHostView_->DeallocFakePluginWindowHandle(pluginHandle_);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)setCutoutRects:(NSArray*)cutout_rects {
  cutoutRects_.reset([cutout_rects copy]);
}

- (void)onRenderWidgetHostViewGone {
  if (!renderWidgetHostView_)
    return;

  // Deallocate the plugin handle while we still can.
  renderWidgetHostView_->DeallocFakePluginWindowHandle(pluginHandle_);
  renderWidgetHostView_ = NULL;
}

- (void)drawRect:(NSRect)rect {
  TRACE_EVENT0("browser", "AcceleratedPluginView::drawRect");
  const NSRect* dirtyRects;
  int dirtyRectCount;
  [self getRectsBeingDrawn:&dirtyRects count:&dirtyRectCount];

  {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;

    // Mask out any cutout rects--somewhat counterintuitively cutout rects are
    // places where clearColor is *not* drawn. The trick is that drawing nothing
    // lets the parent view (i.e., the web page) show through, whereas drawing
    // clearColor punches a hole in the window (letting OpenGL show through).
    if ([cutoutRects_.get() count] > 0) {
      NSBezierPath* path = [NSBezierPath bezierPath];
      // Trace the bounds clockwise to give a base clip rect of the whole view.
      NSRect bounds = [self bounds];
      [path moveToPoint:bounds.origin];
      [path lineToPoint:NSMakePoint(NSMinX(bounds), NSMaxY(bounds))];
      [path lineToPoint:NSMakePoint(NSMaxX(bounds), NSMaxY(bounds))];
      [path lineToPoint:NSMakePoint(NSMaxX(bounds), NSMinY(bounds))];
      [path closePath];

      // Then trace each cutout rect counterclockwise to remove that region from
      // the clip region.
      for (NSValue* rectWrapper in cutoutRects_.get()) {
        [path appendBezierPathWithRect:[rectWrapper rectValue]];
      }

      [path addClip];
    }

    // Punch a hole so that the OpenGL view shows through.
    [[NSColor clearColor] set];
    NSRectFillList(dirtyRects, dirtyRectCount);
  }

  [self drawView];
}

- (void)rightMouseDown:(NSEvent*)event {
  // The NSResponder documentation: "Note: The NSView implementation of this
  // method does not pass the message up the responder chain, it handles it
  // directly."
  // That's bad, we want the next responder (RWHVMac) to handle this event to
  // dispatch it to the renderer.
  [[self nextResponder] rightMouseDown:event];
}

- (void)globalFrameDidChange:(NSNotification*)notification {
  // Short-circuit recursive calls.
  if (handlingGlobalFrameDidChange_)
    return;

  handlingGlobalFrameDidChange_ = YES;

  if (glContext_) {
    // This call to -update can call -globalFrameDidChange: again, see
    // http://crbug.com/55754 comments 22 and 24.
    [glContext_ update];

    // You would think that -update updates the viewport. You would be wrong.
    CGLSetCurrentContext(cglContext_);
    NSSize size = [self frame].size;
    glViewport(0, 0, size.width, size.height);
    CGLSetCurrentContext(0);
  }

  handlingGlobalFrameDidChange_ = NO;
}

- (void)prepareForGLRendering {
  TRACE_EVENT0("browser", "AcceleratedPluginView::prepareForGLRendering");

  // If we're using OpenGL, make sure it is connected.
  if ([glContext_ view] != self) {
    [glContext_ setView:self];
  }
}

- (void)renewGState {
  TRACE_EVENT0("browser", "AcceleratedPluginView::renewGState");
  // Synchronize with window server to avoid flashes or corrupt drawing.
  [[self window] disableScreenUpdatesUntilFlush];
  [self globalFrameDidChange:nil];
  [super renewGState];
}

- (void)setUpGState {
  TRACE_EVENT0("browser", "AcceleratedPluginView::setUpGState");
  [self prepareForGLRendering];
}

// TODO(jbates): is lockFocus needed anymore? it seems redundant with
//               setUpGState in traces.
- (void)lockFocus {
  TRACE_EVENT0("browser", "AcceleratedPluginView::lockFocus");
  [super lockFocus];
  [self prepareForGLRendering];
  [glContext_ makeCurrentContext];
}

- (void)viewDidUnhide {
  TRACE_EVENT0("browser", "AcceleratedPluginView::viewDidUnhide");
  [super viewDidUnhide];

  // Delay context creation until view unhide, see http://crbug.com/109151
  if (renderWidgetHostView_ && !glContext_) {
    [self initOpenGLContext];
  }
}

- (BOOL)acceptsFirstResponder {
  // Accept first responder if the first responder isn't the RWHVMac, and if the
  // RWHVMac accepts first responder.  If the RWHVMac does not accept first
  // responder, do not accept on its behalf.
  return ([[self window] firstResponder] != [self superview] &&
          [[self superview] acceptsFirstResponder]);
}

- (BOOL)becomeFirstResponder {
  // Delegate first responder to the RWHVMac.
  [[self window] makeFirstResponder:[self superview]];
  return YES;
}

- (void)viewDidMoveToSuperview {
  TRACE_EVENT0("browser", "AcceleratedPluginView::viewDidMoveToSuperview");
  if (![self superview])
    [self onRenderWidgetHostViewGone];
}
@end  // @implementation AcceleratedPluginView

@implementation AcceleratedPluginView(Private)

- (void)initOpenGLContext {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser", "AcceleratedPluginViewMac::initOpenGLContext");
  std::vector<NSOpenGLPixelFormatAttribute> attributes;
  attributes.push_back(NSOpenGLPFADoubleBuffer);
  if (gfx::GLContext::SupportsDualGpus())
    attributes.push_back(NSOpenGLPFAAllowOfflineRenderers);
  attributes.push_back(0);

  glPixelFormat_.reset([[NSOpenGLPixelFormat alloc]
                         initWithAttributes:&attributes.front()]);
  if (!glPixelFormat_)
    LOG(ERROR) << "NSOpenGLPixelFormat initWithAttributes failed";

  glContext_.reset([[NSOpenGLContext alloc] initWithFormat:glPixelFormat_
                                              shareContext:nil]);
  if (!glContext_)
    LOG(ERROR) << "NSOpenGLContext initWithFormat failed";

  // We "punch a hole" in the window, and have the WindowServer render the
  // OpenGL surface underneath so we can draw over it.
  GLint belowWindow = -1;
  [glContext_ setValues:&belowWindow forParameter:NSOpenGLCPSurfaceOrder];

  cglContext_ = (CGLContextObj)[glContext_ CGLContextObj];
  if (!cglContext_)
    LOG(ERROR) << "CGLContextObj failed";

  cglPixelFormat_ = (CGLPixelFormatObj)[glPixelFormat_ CGLPixelFormatObj];
  if (!cglPixelFormat_)
    LOG(ERROR) << "CGLPixelFormatObj failed";

  // Draw at beam vsync.
  GLint swapInterval;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    swapInterval = 0;
  else
    swapInterval = 1;
  [glContext_ setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
}

@end  // @implementation AcceleratedPluginView(Private)
