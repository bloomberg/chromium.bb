// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/accelerated_plugin_view_mac.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/common/chrome_switches.h"
#include "ui/gfx/gl/gl_switches.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@implementation AcceleratedPluginView
@synthesize cachedSize = cachedSize_;

- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime {
  // There is no autorelease pool when this method is called because it will be
  // called from a background thread.
  base::mac::ScopedNSAutoreleasePool pool;

  bool sendAck = (rendererId_ != 0 || routeId_ != 0);
  uint64 currentSwapBuffersCount = swapBuffersCount_;
  if (currentSwapBuffersCount == acknowledgedSwapBuffersCount_) {
    return kCVReturnSuccess;
  }

  [self drawView];

  acknowledgedSwapBuffersCount_ = currentSwapBuffersCount;
  if (sendAck && renderWidgetHostView_) {
    renderWidgetHostView_->AcknowledgeSwapBuffers(
        rendererId_,
        routeId_,
        gpuHostId_,
        acknowledgedSwapBuffersCount_);
  }

  return kCVReturnSuccess;
}

// This is the renderer output callback function
static CVReturn DrawOneAcceleratedPluginCallback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp* now,
    const CVTimeStamp* outputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags* flagsOut,
    void* displayLinkContext) {
  CVReturn result =
      [(AcceleratedPluginView*)displayLinkContext getFrameForTime:outputTime];
  return result;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r
                         pluginHandle:(gfx::PluginWindowHandle)pluginHandle {
  if ((self = [super initWithFrame:NSZeroRect])) {
    renderWidgetHostView_ = r;
    pluginHandle_ = pluginHandle;
    cachedSize_ = NSZeroSize;
    swapBuffersCount_ = 0;
    acknowledgedSwapBuffersCount_ = 0;
    rendererId_ = 0;
    routeId_ = 0;
    gpuHostId_ = 0;

    [self setAutoresizingMask:NSViewMaxXMargin|NSViewMinYMargin];

    NSOpenGLPixelFormatAttribute attributes[] =
        { NSOpenGLPFAAccelerated, NSOpenGLPFADoubleBuffer, 0};

    glPixelFormat_.reset([[NSOpenGLPixelFormat alloc]
        initWithAttributes:attributes]);
    glContext_.reset([[NSOpenGLContext alloc] initWithFormat:glPixelFormat_
                                                shareContext:nil]);

    // We "punch a hole" in the window, and have the WindowServer render the
    // OpenGL surface underneath so we can draw over it.
    GLint belowWindow = -1;
    [glContext_ setValues:&belowWindow forParameter:NSOpenGLCPSurfaceOrder];

    cglContext_ = (CGLContextObj)[glContext_ CGLContextObj];
    cglPixelFormat_ = (CGLPixelFormatObj)[glPixelFormat_ CGLPixelFormatObj];

    // Draw at beam vsync.
    GLint swapInterval;
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
      swapInterval = 0;
    else
      swapInterval = 1;
    [glContext_ setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

    // Set up a display link to do OpenGL rendering on a background thread.
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink_);
  }
  return self;
}

- (void)dealloc {
  CVDisplayLinkRelease(displayLink_);
  if (renderWidgetHostView_)
    renderWidgetHostView_->DeallocFakePluginWindowHandle(pluginHandle_);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)drawView {
  TRACE_EVENT1("browser", "AcceleratedPluginViewMac::drawView",
      "frameNum", swapBuffersCount_);
  // Called on a background thread. Synchronized via the CGL context lock.
  CGLLockContext(cglContext_);

  if (renderWidgetHostView_) {
    // TODO(thakis): Pixel or view coordinates for size?
    renderWidgetHostView_->DrawAcceleratedSurfaceInstance(
        cglContext_, pluginHandle_, [self cachedSize]);
  }

  CGLFlushDrawable(cglContext_);
  CGLSetCurrentContext(0);
  CGLUnlockContext(cglContext_);
}

- (void)setCutoutRects:(NSArray*)cutout_rects {
  cutoutRects_.reset([cutout_rects copy]);
}

- (void)updateSwapBuffersCount:(uint64)count
                  fromRenderer:(int)rendererId
                       routeId:(int32)routeId
                     gpuHostId:(int)gpuHostId {
  if (rendererId == 0 && routeId == 0) {
    // This notification is coming from a plugin process, for which we
    // don't have flow control implemented right now. Fake up a swap
    // buffers count so that we can at least skip useless renders.
    ++swapBuffersCount_;
  } else {
    rendererId_ = rendererId;
    routeId_ = routeId;
    gpuHostId_ = gpuHostId;
    swapBuffersCount_ = count;
  }
}

- (void)onRenderWidgetHostViewGone {
  if (!renderWidgetHostView_)
    return;

  CGLLockContext(cglContext_);
  // Deallocate the plugin handle while we still can.
  renderWidgetHostView_->DeallocFakePluginWindowHandle(pluginHandle_);
  renderWidgetHostView_ = NULL;
  CGLUnlockContext(cglContext_);
}

- (void)drawRect:(NSRect)rect {
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
  globalFrameDidChangeCGLLockCount_++;
  CGLLockContext(cglContext_);
  // This call to -update can call -globalFrameDidChange: again, see
  // http://crbug.com/55754 comments 22 and 24.
  [glContext_ update];

  // You would think that -update updates the viewport. You would be wrong.
  CGLSetCurrentContext(cglContext_);
  NSSize size = [self frame].size;
  glViewport(0, 0, size.width, size.height);

  CGLSetCurrentContext(0);
  CGLUnlockContext(cglContext_);
  globalFrameDidChangeCGLLockCount_--;

  if (globalFrameDidChangeCGLLockCount_ == 0) {
    // Make sure the view is synchronized with the correct display.
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(
       displayLink_, cglContext_, cglPixelFormat_);
  }
}

- (void)renewGState {
  // Synchronize with window server to avoid flashes or corrupt drawing.
  [[self window] disableScreenUpdatesUntilFlush];
  [self globalFrameDidChange:nil];
  [super renewGState];
}

- (void)lockFocus {
  [super lockFocus];

  // If we're using OpenGL, make sure it is connected and that the display link
  // is running.
  if ([glContext_ view] != self) {
    [glContext_ setView:self];

    [[NSNotificationCenter defaultCenter]
         addObserver:self
            selector:@selector(globalFrameDidChange:)
                name:NSViewGlobalFrameDidChangeNotification
              object:self];
    CVDisplayLinkSetOutputCallback(
        displayLink_, &DrawOneAcceleratedPluginCallback, self);
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(
        displayLink_, cglContext_, cglPixelFormat_);
    CVDisplayLinkStart(displayLink_);
  }
  [glContext_ makeCurrentContext];
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  // Stop the display link thread while the view is not visible.
  if (newWindow) {
    if (displayLink_ && !CVDisplayLinkIsRunning(displayLink_))
      CVDisplayLinkStart(displayLink_);
  } else {
    if (displayLink_ && CVDisplayLinkIsRunning(displayLink_))
      CVDisplayLinkStop(displayLink_);
  }

  // Inform the window hosting this accelerated view that it needs to be
  // transparent.
  if (![self isHiddenOrHasHiddenAncestor]) {
    if ([[self window] respondsToSelector:@selector(underlaySurfaceRemoved)])
      [static_cast<id>([self window]) underlaySurfaceRemoved];
    if ([newWindow respondsToSelector:@selector(underlaySurfaceAdded)])
      [static_cast<id>(newWindow) underlaySurfaceAdded];
  }
}

- (void)viewDidHide {
  [super viewDidHide];

  if ([[self window] respondsToSelector:@selector(underlaySurfaceRemoved)]) {
    [static_cast<id>([self window]) underlaySurfaceRemoved];
  }
}

- (void)viewDidUnhide {
  [super viewDidUnhide];

  if ([[self window] respondsToSelector:@selector(underlaySurfaceRemoved)]) {
    [static_cast<id>([self window]) underlaySurfaceAdded];
  }
}

- (void)setFrame:(NSRect)frameRect {
  TRACE_EVENT0("browser", "AcceleratedPluginViewMac::setFrame");
  [self setCachedSize:frameRect.size];
  [super setFrame:frameRect];
}

- (void)setFrameSize:(NSSize)newSize {
  [self setCachedSize:newSize];
  [super setFrameSize:newSize];
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
  if (![self superview])
    [self onRenderWidgetHostViewGone];
}
@end

