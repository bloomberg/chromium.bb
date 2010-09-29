// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <QuartzCore/QuartzCore.h>

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "chrome/browser/chrome_thread.h"
#include "app/surface/io_surface_support_mac.h"
#import "base/chrome_application_mac.h"
#include "base/command_line.h"
#include "base/histogram.h"
#include "base/logging.h"
#import "base/scoped_nsautorelease_pool.h"
#import "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_trial.h"
#import "chrome/browser/cocoa/rwhvm_editcommand_helper.h"
#import "chrome/browser/cocoa/view_id_util.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/renderer_host/backing_store_mac.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/WebKit/chromium/public/mac/WebInputEventFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webmenurunner_mac.h"

using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

static inline int ToWebKitModifiers(NSUInteger flags) {
  int modifiers = 0;
  if (flags & NSControlKeyMask) modifiers |= WebInputEvent::ControlKey;
  if (flags & NSShiftKeyMask) modifiers |= WebInputEvent::ShiftKey;
  if (flags & NSAlternateKeyMask) modifiers |= WebInputEvent::AltKey;
  if (flags & NSCommandKeyMask) modifiers |= WebInputEvent::MetaKey;
  return modifiers;
}

@interface RenderWidgetHostViewCocoa (Private)
+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event;
- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r;
- (void)keyEvent:(NSEvent *)theEvent wasKeyEquivalent:(BOOL)equiv;
- (void)cancelChildPopups;
- (void)attachPluginLayer;
@end

// This API was published since 10.6. Provide the declaration so it can be
// // called below when building with the 10.5 SDK.
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_5
@class NSTextInputContext;
@interface NSResponder (AppKitDetails)
- (NSTextInputContext *)inputContext;
@end
#endif

namespace {

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

// TODO(suzhe): Upstream this function.
WebKit::WebColor WebColorFromNSColor(NSColor *color) {
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];

  return
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * a)), 255)) << 24 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * r)), 255)) << 16 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * g)), 255)) << 8  |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * b)), 255));
}

// Extract underline information from an attributed string.
// Mostly copied from third_party/WebKit/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(
    NSAttributedString* string,
    std::vector<WebKit::WebCompositionUnderline>* underlines) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
      WebKit::WebColor color = SK_ColorBLACK;
      if (NSColor *colorAttr =
          [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = WebColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      underlines->push_back(WebKit::WebCompositionUnderline(
          range.location, NSMaxRange(range), color, [style intValue] > 1));
    }
    i = range.location + range.length;
  }
}

// EnablePasswordInput() and DisablePasswordInput() are copied from
// enableSecureTextInput() and disableSecureTextInput() functions in
// third_party/WebKit/WebCore/platform/SecureTextInput.cpp
// But we don't call EnableSecureEventInput() and DisableSecureEventInput()
// here, because they are already called in webkit and they are system wide
// functions.
void EnablePasswordInput() {
  CFArrayRef inputSources = TISCreateASCIICapableInputSourceList();
  TSMSetDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag,
                         sizeof(CFArrayRef), &inputSources);
  CFRelease(inputSources);
}

void DisablePasswordInput() {
  TSMRemoveDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag);
}

}  // namespace

// AcceleratedPluginView ------------------------------------------------------

// This subclass of NSView hosts the output of accelerated plugins on
// the page.

// Informal protocol implemented by windows that need to be informed explicitly
// about underlay surfaces.
@interface NSObject (UnderlayableSurface)
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;
@end

@interface AcceleratedPluginView : NSView {
  scoped_nsobject<NSOpenGLPixelFormat> glPixelFormat_;
  CGLPixelFormatObj cglPixelFormat_;  // weak, backed by |glPixelFormat_|.
  scoped_nsobject<NSOpenGLContext> glContext_;
  CGLContextObj cglContext_;  // weak, backed by |glContext_|.

  CVDisplayLinkRef displayLink_;  // Owned by us.

  RenderWidgetHostViewMac* renderWidgetHostView_;  // weak
  gfx::PluginWindowHandle pluginHandle_;  // weak

  // True if the backing IO surface was updated since we last painted.
  BOOL surfaceWasSwapped_;

  // Cocoa methods can only be called on the main thread, so have a copy of the
  // view's size, since it's required on the displaylink thread.
  NSSize cachedSize_;

  // -globalFrameDidChange: can be called recursively, this counts how often it
  // holds the CGL lock.
  int globalFrameDidChangeCGLLockCount_;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r
                         pluginHandle:(gfx::PluginWindowHandle)pluginHandle;
- (void)drawView;

// This _must_ be atomic, since it's accessed from several threads.
@property BOOL surfaceWasSwapped;

// This _must_ be atomic, since it's accessed from several threads.
@property NSSize cachedSize;
@end

@implementation AcceleratedPluginView
@synthesize surfaceWasSwapped = surfaceWasSwapped_;
@synthesize cachedSize = cachedSize_;

- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime {
  // There is no autorelease pool when this method is called because it will be
  // called from a background thread.
  base::ScopedNSAutoreleasePool pool;

  if (![self surfaceWasSwapped])
    return kCVReturnSuccess;

  [self drawView];
  [self setSurfaceWasSwapped:NO];
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

    [self setAutoresizingMask:NSViewMaxXMargin|NSViewMinYMargin];

    NSOpenGLPixelFormatAttribute attributes[] =
        { NSOpenGLPFAAccelerated, NSOpenGLPFADoubleBuffer, 0};

    glPixelFormat_.reset([[NSOpenGLPixelFormat alloc]
        initWithAttributes:attributes]);
    glContext_.reset([[NSOpenGLContext alloc] initWithFormat:glPixelFormat_
                                                shareContext:nil]);

    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableHolePunching)) {
      // We "punch a hole" in the window, and have the WindowServer render the
      // OpenGL surface underneath so we can draw over it.
      GLint belowWindow = -1;
      [glContext_ setValues:&belowWindow forParameter:NSOpenGLCPSurfaceOrder];
    }

    cglContext_ = (CGLContextObj)[glContext_ CGLContextObj];
    cglPixelFormat_ = (CGLPixelFormatObj)[glPixelFormat_ CGLPixelFormatObj];

    // Draw at beam vsync.
    GLint swapInterval = 1;
    [glContext_ setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

    // Set up a display link to do OpenGL rendering on a background thread.
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink_);
  }
  return self;
}

- (void)dealloc {
  CVDisplayLinkRelease(displayLink_);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)drawView {
  // Called on a background thread. Synchronized via the CGL context lock.
  CGLLockContext(cglContext_);

  // TODO(thakis): Pixel or view coordinates for size?
  renderWidgetHostView_->DrawAcceleratedSurfaceInstance(
      cglContext_, pluginHandle_, [self cachedSize]);

  CGLFlushDrawable(cglContext_);
  CGLUnlockContext(cglContext_);
}

- (void)drawRect:(NSRect)rect {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableHolePunching)) {
    const NSRect* dirtyRects;
    int dirtyRectCount;
    [self getRectsBeingDrawn:&dirtyRects count:&dirtyRectCount];

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
  [glContext_ update];

  // You would think that -update updates the viewport. You would be wrong.
  CGLSetCurrentContext(cglContext_);
  NSSize size = [self frame].size;
  glViewport(0, 0, size.width, size.height);

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

  // If hole pushing is enabled, inform the window hosing this accelerated view
  // that it needs to be opaque.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableHolePunching)) {
    if ([[self window] respondsToSelector:@selector(underlaySurfaceRemoved)]) {
      [static_cast<id>([self window]) underlaySurfaceRemoved];
    }

    if ([newWindow respondsToSelector:@selector(underlaySurfaceAdded)]) {
      [static_cast<id>(newWindow) underlaySurfaceAdded];
    }
  }
}

- (void)setFrame:(NSRect)frameRect {
  [self setCachedSize:frameRect.size];
  [super setFrame:frameRect];
}

- (void)setFrameSize:(NSSize)newSize {
  [self setCachedSize:newSize];
  [super setFrameSize:newSize];
}
@end

// RenderWidgetHostView --------------------------------------------------------

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

// static
RenderWidgetHostView* RenderWidgetHostView::
    GetRenderWidgetHostViewFromNativeView(gfx::NativeView native_view) {
  // TODO(port)
  NOTREACHED() <<
      "RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView not"
      "implemented";
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget)
    : render_widget_host_(widget),
      about_to_validate_and_paint_(false),
      call_set_needs_display_in_rect_pending_(false),
      text_input_type_(WebKit::WebTextInputTypeNone),
      is_loading_(false),
      is_hidden_(false),
      is_popup_menu_(false),
      shutdown_factory_(this),
      parent_view_(NULL) {
  // |cocoa_view_| owns us and we will be deleted when |cocoa_view_| goes away.
  // Since we autorelease it, our caller must put |native_view()| into the view
  // hierarchy right after calling us.
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];
  render_widget_host_->set_view(this);

  // Turn on accessibility only if one or both of these flags is true.
  renderer_accessible_ = IsVoiceOverRunning() ||
                             CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kForceRendererAccessibility);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  bool activatable = popup_type_ == WebKit::WebPopupTypeNone;
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:activatable ? YES : NO];
  [parent_host_view->GetNativeView() addSubview:cocoa_view_];

  NSPoint global_origin = NSPointFromCGPoint(pos.origin().ToCGPoint());
  if ([[NSScreen screens] count] > 0) {
    global_origin.y = [[[NSScreen screens] objectAtIndex:0] frame].size.height -
        pos.height() - global_origin.y;
  }
  NSPoint window_origin =
      [[cocoa_view_ window] convertScreenToBase:global_origin];
  NSPoint view_origin =
      [cocoa_view_ convertPoint:window_origin fromView:nil];
  NSRect initial_frame = NSMakeRect(view_origin.x,
                                    view_origin.y,
                                    pos.width(),
                                    pos.height());
  [cocoa_view_ setFrame:initial_frame];
}

void RenderWidgetHostViewMac::InitAsFullscreen(
    RenderWidgetHostView* parent_host_view) {
  NOTIMPLEMENTED() << "Full screen not implemented on Mac";
}

RenderWidgetHost* RenderWidgetHostViewMac::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewMac::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  if (tab_switch_paint_time_.is_null())
    tab_switch_paint_time_ = base::TimeTicks::Now();
  is_hidden_ = false;
  render_widget_host_->WasRestored();
}

void RenderWidgetHostViewMac::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  render_widget_host_->WasHidden();
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  if (is_hidden_)
    return;

  // During the initial creation of the RenderWidgetHostView in
  // TabContents::CreateRenderViewForRenderManager, SetSize is called with an
  // empty size. In the Windows code flow, it is not ignored because subsequent
  // sizing calls from the OS flow through TCVW::WasSized which calls SetSize()
  // again. On Cocoa, we rely on the Cocoa view struture and resizer flags to
  // keep things sized properly. On the other hand, if the size is not empty
  // then this is a valid request for a pop-up.
  if (size.IsEmpty())
    return;

  // Do conversions to upper-left origin, as "set size" means keep the
  // upper-left corner pinned. If the new size is valid, this is a popup whose
  // superview is another RenderWidgetHostViewCocoa, but even if it's directly
  // in a TabContentsViewCocoa, they're both BaseViews.
  gfx::Rect rect =
      [(BaseView*)[cocoa_view_ superview] flipNSRectToRect:[cocoa_view_ frame]];
  rect.set_width(size.width());
  rect.set_height(size.height());
  [cocoa_view_ setFrame:
      [(BaseView*)[cocoa_view_ superview] flipRectToNSRect:rect]];
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() {
  return native_view();
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const std::vector<webkit_glue::WebPluginGeometry>& moves) {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Handle movement of accelerated plugins, which are the only "windowed"
  // plugins that exist on the Mac.
  for (std::vector<webkit_glue::WebPluginGeometry>::const_iterator iter =
           moves.begin();
       iter != moves.end();
       ++iter) {
    webkit_glue::WebPluginGeometry geom = *iter;
    // Ignore bogus moves which claim to move the plugin to (0, 0)
    // with width and height (0, 0)
    if (geom.window_rect.x() == 0 &&
        geom.window_rect.y() == 0 &&
        geom.window_rect.IsEmpty()) {
      continue;
    }

    gfx::Rect rect = geom.window_rect;
    if (geom.visible) {
      rect.set_x(rect.x() + geom.clip_rect.x());
      rect.set_y(rect.y() + geom.clip_rect.y());
      rect.set_width(geom.clip_rect.width());
      rect.set_height(geom.clip_rect.height());
    }

    PluginViewMap::iterator it = plugin_views_.find(geom.window);
    DCHECK(plugin_views_.end() != it);
    if (plugin_views_.end() == it) {
      continue;
    }
    NSRect new_rect([cocoa_view_ flipRectToNSRect:rect]);
    [it->second setFrame:new_rect];
    [it->second setNeedsDisplay:YES];

    plugin_container_manager_.SetPluginContainerGeometry(geom);

    BOOL visible =
        plugin_container_manager_.SurfaceShouldBeVisible(geom.window);
    [it->second setHidden:!visible];
  }
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

void RenderWidgetHostViewMac::Blur() {
  [[cocoa_view_ window] makeFirstResponder:nil];
}

bool RenderWidgetHostViewMac::HasFocus() {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

void RenderWidgetHostViewMac::Show() {
  [cocoa_view_ setHidden:NO];

  DidBecomeSelected();
}

void RenderWidgetHostViewMac::Hide() {
  [cocoa_view_ setHidden:YES];

  WasHidden();
}

bool RenderWidgetHostViewMac::IsShowing() {
  return ![cocoa_view_ isHidden];
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() const {
  return [cocoa_view_ flipNSRectToRect:[cocoa_view_ bounds]];
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::UpdateCursorIfOverSelf() {
  // Do something special (as Win Chromium does) for arrow cursor while loading
  // a page? TODO(avi): decide
  // Can we synchronize to the event stream? Switch to -[NSWindow
  // mouseLocationOutsideOfEventStream] if we cannot. TODO(avi): test and see
  NSEvent* event = [[cocoa_view_ window] currentEvent];
  if ([event window] != [cocoa_view_ window])
    return;

  NSPoint event_location = [event locationInWindow];
  NSPoint local_point = [cocoa_view_ convertPoint:event_location fromView:nil];

  if (!NSPointInRect(local_point, [cocoa_view_ bounds]))
    return;

  NSCursor* ns_cursor = current_cursor_.GetCursor();
  [ns_cursor set];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::ImeUpdateTextInputState(
    WebKit::WebTextInputType type,
    const gfx::Rect& caret_rect) {
  if (text_input_type_ != type) {
    text_input_type_ = type;
    if (HasFocus())
      SetTextInputActive(true);
  }

  // We need to convert the coordinate of the cursor rectangle sent from the
  // renderer and save it. Our input method backend uses a coordinate system
  // whose origin is the upper-left corner of this view. On the other hand,
  // Cocoa uses a coordinate system whose origin is the lower-left corner of
  // this view. So, we convert the cursor rectangle and save it.
  [cocoa_view_ setCaretRect:[cocoa_view_ flipRectToNSRect:caret_rect]];
}

void RenderWidgetHostViewMac::ImeCancelComposition() {
  [cocoa_view_ cancelComposition];
}

void RenderWidgetHostViewMac::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (is_hidden_)
    return;

  std::vector<gfx::Rect> rects(copy_rects);

  // Because the findbar might be open, we cannot use scrollRect:by: here.  For
  // now, simply mark all of scroll rect as dirty.
  if (!scroll_rect.IsEmpty())
    rects.push_back(scroll_rect);

  for (size_t i = 0; i < rects.size(); ++i) {
    NSRect ns_rect = [cocoa_view_ flipRectToNSRect:rects[i]];

    if (about_to_validate_and_paint_) {
      // As much as we'd like to use -setNeedsDisplayInRect: here, we can't.
      // We're in the middle of executing a -drawRect:, and as soon as it
      // returns Cocoa will clear its record of what needs display. We instead
      // use |performSelector:| to call |setNeedsDisplayInRect:| after returning
      // to the main loop, at which point |drawRect:| is no longer on the stack.
      DCHECK([NSThread isMainThread]);
      if (!call_set_needs_display_in_rect_pending_) {
        [cocoa_view_ performSelector:@selector(callSetNeedsDisplayInRect)
                     withObject:nil
                     afterDelay:0];
        call_set_needs_display_in_rect_pending_ = true;
        invalid_rect_ = ns_rect;
      } else {
        // The old invalid rect is probably invalid now, since the view has most
        // likely been resized, but there's no harm in dirtying the union.  In
        // the limit, this becomes equivalent to dirtying the whole view.
        invalid_rect_ = NSUnionRect(invalid_rect_, ns_rect);
      }
    } else {
      [cocoa_view_ setNeedsDisplayInRect:ns_rect];
    }
  }

  if (!about_to_validate_and_paint_)
    [cocoa_view_ displayIfNeeded];
}

void RenderWidgetHostViewMac::RenderViewGone() {
  // TODO(darin): keep this around, and draw sad-tab into it.
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // On Windows, popups are implemented with a popup window style, so that when
  // an event comes in that would "cancel" it, it receives the OnCancelMode
  // message and can kill itself. Alas, on the Mac, views cannot capture events
  // outside of themselves. On Windows, if Destroy is being called on a view,
  // then the event causing the destroy had also cancelled any popups by the
  // time Destroy() was called. On the Mac we have to destroy all the popups
  // ourselves.

  if (!is_popup_menu_) {
    // Depth-first destroy all popups. Use ShutdownHost() to enforce
    // deepest-first ordering.
    for (NSView* subview in [cocoa_view_ subviews]) {
      if (![subview isKindOfClass:[RenderWidgetHostViewCocoa class]])
        continue;  // Skip accelerated views.

      [static_cast<RenderWidgetHostViewCocoa*>(subview)
          renderWidgetHostViewMac]->ShutdownHost();
    }

    // We've been told to destroy.
    [cocoa_view_ retain];
    [cocoa_view_ removeFromSuperview];
    [cocoa_view_ autorelease];
  } else {
    // From the renderer's perspective, the pop-up menu is represented by a
    // RenderWidget. The actual Mac implementation uses a native pop-up menu
    // and doesn't actually make use of the RenderWidgetHostViewCocoa that
    // was allocated to own it in its constructor.  When the pop-up menu goes
    // away, free the RenderWidgetHostViewCocoa. Its deallocation will result
    // in this object's destruction.

    DCHECK([[cocoa_view_ subviews] count] == 0);
    DCHECK([cocoa_view_ superview] == nil);

    [cocoa_view_ autorelease];
  }

  // We get this call just before |render_widget_host_| deletes
  // itself.  But we are owned by |cocoa_view_|, which may be retained
  // by some other code.  Examples are TabContentsViewMac's
  // |latent_focus_view_| and TabWindowController's
  // |cachedContentView_|.
  render_widget_host_ = NULL;
}

// Called from the renderer to tell us what the tooltip text should be. It
// calls us frequently so we need to cache the value to prevent doing a lot
// of repeat work.
void RenderWidgetHostViewMac::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_ && [[cocoa_view_ window] isKeyWindow]) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite. Don't persist the trimmed
    // string, as then the comparison above will always fail and we'll try to
    // set it again every single time the mouse moves.
    std::wstring display_text = tooltip_text_;
    if (tooltip_text_.length() > kMaxTooltipLength)
      display_text = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysWideToNSString(display_text);
    [cocoa_view_ setToolTipAtMousePoint:tooltip_nsstring];
  }
}

//
// RenderWidgetHostViewCocoa uses the stored selection text,
// which implements NSServicesRequests protocol.
//
void RenderWidgetHostViewMac::SelectionChanged(const std::string& text) {
  selected_text_ = text;
}

BackingStore* RenderWidgetHostViewMac::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreMac(render_widget_host_, size);
}

VideoLayer* RenderWidgetHostViewMac::AllocVideoLayer(
    const gfx::Size& size) {
  NOTIMPLEMENTED();
  return NULL;
}

// Display a popup menu for WebKit using Cocoa widgets.
void RenderWidgetHostViewMac::ShowPopupWithItems(
    gfx::Rect bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned) {
  is_popup_menu_ = true;

  // Retain the Cocoa view for the duration of the pop-up so that it can't
  // be dealloced if my Destroy() method is called while the pop-up's up
  // (which would in turn delete me, causing a crash once the -runMenuInView
  // call returns. That's what was happening in <http://crbug.com/33250>).
  scoped_nsobject<RenderWidgetHostViewCocoa> retainedCocoaView
      ([cocoa_view_ retain]);

  NSRect view_rect = [cocoa_view_ bounds];
  NSRect parent_rect = [parent_view_ bounds];
  int y_offset = bounds.y() + bounds.height();
  NSRect position = NSMakeRect(bounds.x(), parent_rect.size.height - y_offset,
                               bounds.width(), bounds.height());

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items
                                                fontSize:item_font_size
                                            rightAligned:right_aligned]);

  {
    // Make sure events can be pumped while the menu is up.
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    chrome_application_mac::ScopedSendingEvent sendingEventScoper;

    // Now run a SYNCHRONOUS NESTED EVENT LOOP until the pop-up is finished.
    [menu_runner runMenuInView:parent_view_
                    withBounds:position
                  initialIndex:selected_item];
  }

  if (!render_widget_host_) {
    // Bad news -- my Destroy() was called while I was off running the menu.
    // Return ASAP, and the release of retainedCocoaView will dealloc my NSView,
    // which will delete me (whew).
    return;
  }

  int window_num = [[parent_view_ window] windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Simulate a menu selection event.
    const WebMouseEvent& mouse_event =
        WebInputEventFactory::mouseEvent(event, cocoa_view_);
    render_widget_host_->ForwardMouseEvent(mouse_event);
  } else {
    // Simulate a menu dismiss event.
    NativeWebKeyboardEvent keyboard_event(event);
    render_widget_host_->ForwardKeyboardEvent(keyboard_event);
  }
}

void RenderWidgetHostViewMac::KillSelf() {
  if (shutdown_factory_.empty()) {
    [cocoa_view_ setHidden:YES];
    MessageLoop::current()->PostTask(FROM_HERE,
        shutdown_factory_.NewRunnableMethod(
            &RenderWidgetHostViewMac::ShutdownHost));
  }
}

gfx::PluginWindowHandle
RenderWidgetHostViewMac::AllocateFakePluginWindowHandle(bool opaque,
                                                        bool root) {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Create an NSView to host the plugin's/compositor's pixels.
  gfx::PluginWindowHandle handle =
      plugin_container_manager_.AllocateFakePluginWindowHandle(opaque, root);

  scoped_nsobject<AcceleratedPluginView> plugin_view(
      [[AcceleratedPluginView alloc] initWithRenderWidgetHostViewMac:this
                                                        pluginHandle:handle]);
  [plugin_view setHidden:YES];

  [cocoa_view_ addSubview:plugin_view];
  plugin_views_[handle] = plugin_view;

  return handle;
}

void RenderWidgetHostViewMac::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle window) {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PluginViewMap::iterator it = plugin_views_.find(window);
  DCHECK(plugin_views_.end() != it);
  if (plugin_views_.end() == it) {
    return;
  }
  [it->second removeFromSuperview];
  plugin_views_.erase(it);
  plugin_container_manager_.DestroyFakePluginWindowHandle(window);
}

void RenderWidgetHostViewMac::AcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  plugin_container_manager_.SetSizeAndIOSurface(window,
                                                width,
                                                height,
                                                io_surface_identifier);

  if (plugin_container_manager_.IsRootContainer(window)) {
    // Fake up a WebPluginGeometry for the root window to set the
    // container's size; we will never get a notification from the
    // browser about the root window, only plugins.
    webkit_glue::WebPluginGeometry geom;
    gfx::Rect rect(0, 0, width, height);
    geom.window = window;
    geom.window_rect = rect;
    geom.clip_rect = rect;
    geom.visible = true;
    MovePluginWindows(std::vector<webkit_glue::WebPluginGeometry>(1, geom));
  }
}

void RenderWidgetHostViewMac::AcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  plugin_container_manager_.SetSizeAndTransportDIB(window,
                                                   width,
                                                   height,
                                                   transport_dib);
}

void RenderWidgetHostViewMac::AcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window) {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PluginViewMap::iterator it = plugin_views_.find(window);
  DCHECK(plugin_views_.end() != it);
  if (plugin_views_.end() == it) {
    return;
  }
  DCHECK([it->second isKindOfClass:[AcceleratedPluginView class]]);

  plugin_container_manager_.SetSurfaceWasPaintedTo(window);
  AcceleratedPluginView* view =
      static_cast<AcceleratedPluginView*>(it->second);
  // The surface is hidden until its first paint, to not show gargabe.
  if (plugin_container_manager_.SurfaceShouldBeVisible(window))
    [view setHidden:NO];
  [view setSurfaceWasSwapped:YES];
}

void RenderWidgetHostViewMac::GpuRenderingStateDidChange() {
  CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Plugins are destroyed on page navigate. The compositor layer on the other
  // hand is created on demand and then stays alive until its renderer process
  // dies (usually on cross-domain navigation). Instead, only a flag
  // |is_gpu_rendering_active()| is flipped when the compositor output should be
  // shown/hidden.
  // Show/hide the view belonging to the compositor here.
  plugin_container_manager_.set_gpu_rendering_active(
      GetRenderWidgetHost()->is_gpu_rendering_active());

  gfx::PluginWindowHandle root_handle =
      plugin_container_manager_.root_container_handle();
  if (root_handle != gfx::kNullPluginWindow) {
    PluginViewMap::iterator it = plugin_views_.find(root_handle);
    DCHECK(plugin_views_.end() != it);
    if (plugin_views_.end() == it) {
      return;
    }
    bool visible =
        plugin_container_manager_.SurfaceShouldBeVisible(root_handle);
    [it->second setHidden:!visible];
  }
}

void RenderWidgetHostViewMac::DrawAcceleratedSurfaceInstance(
      CGLContextObj context,
      gfx::PluginWindowHandle plugin_handle,
      NSSize size) {
  // Called on the display link thread.
  CGLSetCurrentContext(context);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // Note that we place the origin at the upper left corner with +y
  // going down
  glOrtho(0, size.width, size.height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  plugin_container_manager_.Draw(context, plugin_handle);
}

void RenderWidgetHostViewMac::ForceTextureReload() {
  plugin_container_manager_.ForceTextureReload();
}

void RenderWidgetHostViewMac::SetVisuallyDeemphasized(bool deemphasized) {
  // Mac uses tab-modal sheets, so this is a no-op.
}

void RenderWidgetHostViewMac::ShutdownHost() {
  shutdown_factory_.RevokeAll();
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

bool RenderWidgetHostViewMac::IsVoiceOverRunning() {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  [user_defaults addSuiteNamed:@"com.apple.universalaccess"];
  return 1 == [user_defaults integerForKey:@"voiceOverOnOffKey"];
}

namespace {

// Adjusts an NSRect in Cocoa screen coordinates to have an origin in the upper
// left of the primary screen (Carbon coordinates), and stuffs it into a
// gfx::Rect.
gfx::Rect FlipNSRectToRectScreen(const NSRect rect) {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  if ([[NSScreen screens] count] > 0) {
    new_rect.set_y([[[NSScreen screens] objectAtIndex:0] frame].size.height -
                   new_rect.y() - new_rect.height());
  }
  return new_rect;
}

// Returns the window that visually contains the given view. This is different
// from [view window] in the case of tab dragging, where the view's owning
// window is a floating panel attached to the actual browser window that the tab
// is visually part of.
NSWindow* ApparentWindowForView(NSView* view) {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = [view window];

  // See if this is a tab drag window. The width check is to distinguish that
  // case from extension popup windows.
  NSWindow* ancestor_window = [enclosing_window parentWindow];
  if (ancestor_window && (NSWidth([enclosing_window frame]) ==
                          NSWidth([ancestor_window frame]))) {
    enclosing_window = ancestor_window;
  }

  return enclosing_window;
}

}  // namespace

gfx::Rect RenderWidgetHostViewMac::GetWindowRect() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return gfx::Rect();

  NSRect bounds = [cocoa_view_ bounds];
  bounds = [cocoa_view_ convertRect:bounds toView:nil];
  bounds.origin = [enclosing_window convertBaseToScreen:bounds.origin];
  return FlipNSRectToRectScreen(bounds);
}

gfx::Rect RenderWidgetHostViewMac::GetRootWindowRect() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return gfx::Rect();

  NSRect bounds = [enclosing_window frame];
  return FlipNSRectToRectScreen(bounds);
}

void RenderWidgetHostViewMac::SetActive(bool active) {
  if (render_widget_host_)
    render_widget_host_->SetActive(active);
  if (HasFocus())
    SetTextInputActive(active);
}

void RenderWidgetHostViewMac::SetWindowVisibility(bool visible) {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_SetWindowVisibility(
        render_widget_host_->routing_id(), visible));
  }
}

void RenderWidgetHostViewMac::WindowFrameChanged() {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_WindowFrameChanged(
        render_widget_host_->routing_id(), GetRootWindowRect(),
        GetWindowRect()));
  }
}

void RenderWidgetHostViewMac::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  if (render_widget_host_)
    render_widget_host_->Send(new ViewMsg_SetBackground(
        render_widget_host_->routing_id(), background));
}

bool RenderWidgetHostViewMac::ContainsNativeView(
    gfx::NativeView native_view) const {
  // TODO(port)
  NOTREACHED() <<
    "RenderWidgetHostViewMac::ContainsNativeView not implemented.";
  return false;
}

void RenderWidgetHostViewMac::UpdateAccessibilityTree(
    const webkit_glue::WebAccessibility& tree) {
  if (renderer_accessible_) {
    [cocoa_view_ setAccessibilityTree:tree];
  }
}

void RenderWidgetHostViewMac::SetTextInputActive(bool active) {
  if (active) {
    if (text_input_type_ == WebKit::WebTextInputTypePassword)
      EnablePasswordInput();
    else
      DisablePasswordInput();
  } else {
    if (text_input_type_ == WebKit::WebTextInputTypePassword)
      DisablePasswordInput();
  }
}

// RenderWidgetHostViewCocoa ---------------------------------------------------

@implementation RenderWidgetHostViewCocoa

@synthesize caretRect = caretRect_;

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    editCommand_helper_.reset(new RWHVMEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    renderWidgetHostView_.reset(r);
    canBeKeyView_ = YES;
    closeOnDeactivate_ = NO;

    rendererAccessible_ =
          !CommandLine::ForCurrentProcess()->HasSwitch(
                    switches::kDisableRendererAccessibility);
    accessibilityRequested_ = NO;
    accessibilityReceived_ = NO;
  }
  return self;
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  // Don't cancel child popups; killing them on a mouse click would prevent the
  // user from positioning the insertion point in the text field spawning the
  // popup. A click outside the text field would cause the text field to drop
  // the focus, and then EditorClientImpl::textFieldDidEndEditing() would cancel
  // the popup anyway, so we're OK.

  NSEventType type = [theEvent type];
  if (type == NSLeftMouseDown)
    hasOpenMouseDown_ = YES;
  else if (type == NSLeftMouseUp)
    hasOpenMouseDown_ = NO;

  // TODO(suzhe): We should send mouse events to the input method first if it
  // wants to handle them. But it won't work without implementing method
  // - (NSUInteger)characterIndexForPoint:.
  // See: http://code.google.com/p/chromium/issues/detail?id=47141
  // Instead of sending mouse events to the input method first, we now just
  // simply confirm all ongoing composition here.
  if (type == NSLeftMouseDown || type == NSRightMouseDown ||
      type == NSOtherMouseDown) {
    [self confirmComposition];
  }

  const WebMouseEvent& event =
      WebInputEventFactory::mouseEvent(theEvent, self);

  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardMouseEvent(event);
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  // |performKeyEquivalent:| is sent to all views of a window, not only down the
  // responder chain (cf. "Handling Key Equivalents" in
  // http://developer.apple.com/mac/library/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html
  // ). We only want to handle key equivalents if we're first responder.
  if ([[self window] firstResponder] != self)
    return NO;

  // If we return |NO| from this function, cocoa will send the key event to
  // the menu and only if the menu does not process the event to |keyDown:|. We
  // want to send the event to a renderer _before_ sending it to the menu, so
  // we need to return |YES| for all events that might be swallowed by the menu.
  // We do not return |YES| for every keypress because we don't get |keyDown:|
  // events for keys that we handle this way.
  NSUInteger modifierFlags = [theEvent modifierFlags];
  if ((modifierFlags & NSCommandKeyMask) == 0) {
    // Make sure the menu does not contain key equivalents that don't
    // contain cmd.
    DCHECK(![[NSApp mainMenu] performKeyEquivalent:theEvent]);
    return NO;
  }

  // Command key combinations are sent via performKeyEquivalent rather than
  // keyDown:. We just forward this on and if WebCore doesn't want to handle
  // it, we let the TabContentsView figure out how to reinject it.
  [self keyEvent:theEvent wasKeyEquivalent:YES];
  return YES;
}

- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  // This is a SPI that AppKit apparently calls after |performKeyEquivalent:|
  // returned NO. If this function returns |YES|, Cocoa sends the event to
  // |keyDown:| instead of doing other things with it. Ctrl-tab will be sent
  // to us instead of doing key view loop control, ctrl-left/right get handled
  // correctly, etc.
  // (However, there are still some keys that Cocoa swallows, e.g. the key
  // equivalent that Cocoa uses for toggling the input langauge. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (void)keyEvent:(NSEvent*)theEvent {
  [self keyEvent:theEvent wasKeyEquivalent:NO];
}

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv {
  DCHECK([theEvent type] != NSKeyDown ||
         !equiv == !([theEvent modifierFlags] & NSCommandKeyMask));

  if ([theEvent type] == NSFlagsChanged) {
    // Ignore NSFlagsChanged events from the NumLock and Fn keys as
    // Safari does in -[WebHTMLView flagsChanged:] (of "WebHTMLView.mm").
    int keyCode = [theEvent keyCode];
    if (!keyCode || keyCode == 10 || keyCode == 63)
      return;
  }

  // Don't cancel child popups; the key events are probably what's triggering
  // the popup in the first place.

  RenderWidgetHost* widgetHost = renderWidgetHostView_->render_widget_host_;
  DCHECK(widgetHost);

  NativeWebKeyboardEvent event(theEvent);

  // We only handle key down events and just simply forward other events.
  if ([theEvent type] != NSKeyDown) {
    widgetHost->ForwardKeyboardEvent(event);

    // Possibly autohide the cursor.
    if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
      [NSCursor setHiddenUntilMouseMoves:YES];

    return;
  }

  scoped_nsobject<RenderWidgetHostViewCocoa> keepSelfAlive([self retain]);

  // Records the current marked text state, so that we can know if the marked
  // text was deleted or not after handling the key down event.
  BOOL oldHasMarkedText = hasMarkedText_;

  // This method should not be called recursively.
  DCHECK(!handlingKeyDown_);

  // Tells insertText: and doCommandBySelector: that we are handling a key
  // down event.
  handlingKeyDown_ = YES;

  // These variables might be set when handling the keyboard event.
  // Clear them here so that we can know whether they have changed afterwards.
  textToBeInserted_.clear();
  markedText_.clear();
  underlines_.clear();
  unmarkTextCalled_ = NO;
  hasEditCommands_ = NO;
  editCommands_.clear();

  // Sends key down events to input method first, then we can decide what should
  // be done according to input method's feedback.
  [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];

  handlingKeyDown_ = NO;

  // Indicates if we should send the key event and corresponding editor commands
  // after processing the input method result.
  BOOL delayEventUntilAfterImeCompostion = NO;

  // To emulate Windows, over-write |event.windowsKeyCode| to VK_PROCESSKEY
  // while an input method is composing or inserting a text.
  // Gmail checks this code in its onkeydown handler to stop auto-completing
  // e-mail addresses while composing a CJK text.
  // If the text to be inserted has only one character, then we don't need this
  // trick, because we'll send the text as a key press event instead.
  if (hasMarkedText_ || oldHasMarkedText || textToBeInserted_.length() > 1) {
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.windowsKeyCode = 0xE5;  // VKEY_PROCESSKEY
    fakeEvent.setKeyIdentifierFromWindowsKeyCode();
    fakeEvent.skip_in_browser = true;
    widgetHost->ForwardKeyboardEvent(fakeEvent);
    // If this key event was handled by the input method, but
    // -doCommandBySelector: (invoked by the call to -interpretKeyEvents: above)
    // enqueued edit commands, then in order to let webkit handle them
    // correctly, we need to send the real key event and corresponding edit
    // commands after processing the input method result.
    // We shouldn't do this if a new marked text was set by the input method,
    // otherwise the new marked text might be cancelled by webkit.
    if (hasEditCommands_ && !hasMarkedText_)
      delayEventUntilAfterImeCompostion = YES;
  } else {
    if (!editCommands_.empty())
      widgetHost->ForwardEditCommandsForNextKeyEvent(editCommands_);
    widgetHost->ForwardKeyboardEvent(event);
  }

  // Calling ForwardKeyboardEvent() could have destroyed the widget. When the
  // widget was destroyed, |renderWidgetHostView_->render_widget_host_| will
  // be set to NULL. So we check it here and return immediately if it's NULL.
  if (!renderWidgetHostView_->render_widget_host_)
    return;

  // Then send keypress and/or composition related events.
  // If there was a marked text or the text to be inserted is longer than 1
  // character, then we send the text by calling ConfirmComposition().
  // Otherwise, if the text to be inserted only contains 1 character, then we
  // can just send a keypress event which is fabricated by changing the type of
  // the keydown event, so that we can retain all necessary informations, such
  // as unmodifiedText, etc. And we need to set event.skip_in_browser to true to
  // prevent the browser from handling it again.
  // Note that, |textToBeInserted_| is a UTF-16 string, but it's fine to only
  // handle BMP characters here, as we can always insert non-BMP characters as
  // text.
  BOOL textInserted = NO;
  if (textToBeInserted_.length() >
      ((hasMarkedText_ || oldHasMarkedText) ? 0u : 1u)) {
    widgetHost->ImeConfirmComposition(textToBeInserted_);
    textInserted = YES;
  }

  // Updates or cancels the composition. If some text has been inserted, then
  // we don't need to cancel the composition explicitly.
  if (hasMarkedText_ && markedText_.length()) {
    // Sends the updated marked text to the renderer so it can update the
    // composition node in WebKit.
    // When marked text is available, |selectedRange_| will be the range being
    // selected inside the marked text.
    widgetHost->ImeSetComposition(markedText_, underlines_,
                                  selectedRange_.location,
                                  NSMaxRange(selectedRange_));
  } else if (oldHasMarkedText && !hasMarkedText_ && !textInserted) {
    if (unmarkTextCalled_)
      widgetHost->ImeConfirmComposition();
    else
      widgetHost->ImeCancelComposition();
  }

  // If the key event was handled by the input method but it also generated some
  // edit commands, then we need to send the real key event and corresponding
  // edit commands here. This usually occurs when the input method wants to
  // finish current composition session but still wants the application to
  // handle the key event. See http://crbug.com/48161 for reference.
  if (delayEventUntilAfterImeCompostion) {
    // If |delayEventUntilAfterImeCompostion| is YES, then a fake key down event
    // with windowsKeyCode == 0xE5 has already been sent to webkit.
    // So before sending the real key down event, we need to send a fake key up
    // event to balance it.
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.type = WebKit::WebInputEvent::KeyUp;
    fakeEvent.skip_in_browser = true;
    widgetHost->ForwardKeyboardEvent(fakeEvent);
    // Not checking |renderWidgetHostView_->render_widget_host_| here because
    // a key event with |skip_in_browser| == true won't be handled by browser,
    // thus it won't destroy the widget.

    if (!editCommands_.empty())
      widgetHost->ForwardEditCommandsForNextKeyEvent(editCommands_);
    widgetHost->ForwardKeyboardEvent(event);

    // Calling ForwardKeyboardEvent() could have destroyed the widget. When the
    // widget was destroyed, |renderWidgetHostView_->render_widget_host_| will
    // be set to NULL. So we check it here and return immediately if it's NULL.
    if (!renderWidgetHostView_->render_widget_host_)
      return;
  }

  // Only send a corresponding key press event if there is no marked text.
  if (!hasMarkedText_) {
    if (!textInserted && textToBeInserted_.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      event.type = WebKit::WebInputEvent::Char;
      event.text[0] = textToBeInserted_[0];
      event.text[1] = 0;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEvent(event);
    } else if ((!textInserted || delayEventUntilAfterImeCompostion) &&
               editCommands_.empty() && [[theEvent characters] length] > 0) {
      // We don't get insertText: calls if ctrl or cmd is down, or the key event
      // generates an insert command. So synthesize a keypress event for these
      // cases, unless the key event generated any other command.
      event.type = WebKit::WebInputEvent::Char;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEvent(event);
    }
  }

  // Possibly autohide the cursor.
  if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
    [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)scrollWheel:(NSEvent *)theEvent {
  [self cancelChildPopups];

  const WebMouseWheelEvent& event =
      WebInputEventFactory::mouseWheelEvent(theEvent, self);
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(event);
}

// See the comment in RenderWidgetHostViewMac::Destroy() about cancellation
// events. On the Mac we must kill popups on outside events, thus this lovely
// case of filicide caused by events on parent views.
- (void)cancelChildPopups {
  // If this view can be the key view, it is not a popup. Therefore, if it has
  // any children, they are popups that need to be canceled.
  if (canBeKeyView_) {
    for (NSView* subview in [self subviews]) {
      if (![subview isKindOfClass:[RenderWidgetHostViewCocoa class]])
        continue;  // Skip plugin views.

      [static_cast<RenderWidgetHostViewCocoa*>(subview)
          renderWidgetHostViewMac]->KillSelf();
    }
  }
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->WasResized();
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->WasResized();
}

- (void)setFrameWithDeferredUpdate:(NSRect)frameRect {
  [super setFrame:frameRect];
  [self performSelector:@selector(renderWidgetHostWasResized)
             withObject:nil
             afterDelay:0];
}

- (void)renderWidgetHostWasResized {
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->WasResized();
}

- (void)callSetNeedsDisplayInRect {
  DCHECK([NSThread isMainThread]);
  DCHECK(renderWidgetHostView_->call_set_needs_display_in_rect_pending_);
  [self setNeedsDisplayInRect:renderWidgetHostView_->invalid_rect_];
  renderWidgetHostView_->call_set_needs_display_in_rect_pending_ = false;
  renderWidgetHostView_->invalid_rect_ = NSZeroRect;
}

// Fills with white the parts of the area to the right and bottom for |rect|
// that intersect |damagedRect|.
- (void)fillBottomRightRemainderOfRect:(gfx::Rect)rect
                             dirtyRect:(gfx::Rect)damagedRect {
  if (damagedRect.right() > rect.right()) {
    int x = std::max(rect.right(), damagedRect.x());
    int y = std::min(rect.bottom(), damagedRect.bottom());
    int width = damagedRect.right() - x;
    int height = damagedRect.y() - y;

    // Extra fun to get around the fact that gfx::Rects can't have
    // negative sizes.
    if (width < 0) {
      x += width;
      width = -width;
    }
    if (height < 0) {
      y += height;
      height = -height;
    }

    NSRect r = [self flipRectToNSRect:gfx::Rect(x, y, width, height)];
    [[NSColor whiteColor] set];
    NSRectFill(r);
  }
  if (damagedRect.bottom() > rect.bottom()) {
    int x = damagedRect.x();
    int y = damagedRect.bottom();
    int width = damagedRect.right() - x;
    int height = std::max(rect.bottom(), damagedRect.y()) - y;

    // Extra fun to get around the fact that gfx::Rects can't have
    // negative sizes.
    if (width < 0) {
      x += width;
      width = -width;
    }
    if (height < 0) {
      y += height;
      height = -height;
    }

    NSRect r = [self flipRectToNSRect:gfx::Rect(x, y, width, height)];
    [[NSColor whiteColor] set];
    NSRectFill(r);
  }
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!renderWidgetHostView_->render_widget_host_) {
    // TODO(shess): Consider using something more noticable?
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    return;
  }

  const gfx::Rect damagedRect([self flipNSRectToRect:dirtyRect]);

  if (renderWidgetHostView_->render_widget_host_->is_gpu_rendering_active()) {
    gfx::Rect gpuRect;

    gfx::PluginWindowHandle root_handle =
       renderWidgetHostView_->plugin_container_manager_.root_container_handle();
    if (root_handle != gfx::kNullPluginWindow) {
      RenderWidgetHostViewMac::PluginViewMap::iterator it =
          renderWidgetHostView_->plugin_views_.find(root_handle);
      DCHECK(it != renderWidgetHostView_->plugin_views_.end());
      if (it != renderWidgetHostView_->plugin_views_.end() &&
          ![it->second isHidden]) {
        NSRect frame = [it->second frame];
        frame.size = [it->second cachedSize];
        gpuRect = [self flipNSRectToRect:frame];
      }
    }

    [self fillBottomRightRemainderOfRect:gpuRect dirtyRect:damagedRect];
    return;
  }

  DCHECK(
      renderWidgetHostView_->render_widget_host_->process()->HasConnection());
  DCHECK(!renderWidgetHostView_->about_to_validate_and_paint_);

  renderWidgetHostView_->about_to_validate_and_paint_ = true;
  BackingStoreMac* backingStore = static_cast<BackingStoreMac*>(
      renderWidgetHostView_->render_widget_host_->GetBackingStore(true));
  renderWidgetHostView_->about_to_validate_and_paint_ = false;

  if (backingStore) {
    gfx::Rect bitmapRect(0, 0,
                         backingStore->size().width(),
                         backingStore->size().height());

    // Specify the proper y offset to ensure that the view is rooted to the
    // upper left corner.  This can be negative, if the window was resized
    // smaller and the renderer hasn't yet repainted.
    int yOffset = NSHeight([self bounds]) - backingStore->size().height();

    gfx::Rect paintRect = bitmapRect.Intersect(damagedRect);
    if (!paintRect.IsEmpty()) {
      // if we have a CGLayer, draw that into the window
      if (backingStore->cg_layer()) {
        CGContextRef context = static_cast<CGContextRef>(
            [[NSGraphicsContext currentContext] graphicsPort]);

        // TODO: add clipping to dirtyRect if it improves drawing performance.
        CGContextDrawLayerAtPoint(context, CGPointMake(0.0, yOffset),
                                  backingStore->cg_layer());
      } else {
        // if we haven't created a layer yet, draw the cached bitmap into
        // the window.  The CGLayer will be created the next time the renderer
        // paints.
        CGContextRef context = static_cast<CGContextRef>(
            [[NSGraphicsContext currentContext] graphicsPort]);
        scoped_cftyperef<CGImageRef> image(
            CGBitmapContextCreateImage(backingStore->cg_bitmap()));
        CGRect imageRect = bitmapRect.ToCGRect();
        imageRect.origin.y = yOffset;
        CGContextDrawImage(context, imageRect, image);
      }
    }

    // Fill the remaining portion of the damagedRect with white
    [self fillBottomRightRemainderOfRect:bitmapRect dirtyRect:damagedRect];

    if (!renderWidgetHostView_->whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks();
    }
    if (!renderWidgetHostView_->tab_switch_paint_time_.is_null()) {
      base::TimeDelta tab_switch_paint_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->tab_switch_paint_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration",
          tab_switch_paint_duration);
      // Reset tab_switch_paint_time_ to 0 so future tab selections are
      // recorded.
      renderWidgetHostView_->tab_switch_paint_time_ = base::TimeTicks();
    }
  } else {
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    if (renderWidgetHostView_->whiteout_start_time_.is_null())
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks::Now();
  }
}

- (BOOL)canBecomeKeyView {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)becomeFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  renderWidgetHostView_->render_widget_host_->Focus();
  renderWidgetHostView_->render_widget_host_->SetInputMethodActive(true);
  renderWidgetHostView_->SetTextInputActive(true);

  // Cancel any onging composition text which was left before we lost focus.
  // TODO(suzhe): We should do it in -resignFirstResponder: method, but
  // somehow that method won't be called when switching among different tabs.
  // See http://crbug.com/47209
  [self cancelComposition];

  return YES;
}

- (BOOL)resignFirstResponder {
  renderWidgetHostView_->SetTextInputActive(false);
  if (!renderWidgetHostView_->render_widget_host_)
    return YES;

  if (closeOnDeactivate_)
    renderWidgetHostView_->KillSelf();

  renderWidgetHostView_->render_widget_host_->SetInputMethodActive(false);
  renderWidgetHostView_->render_widget_host_->Blur();

  // We should cancel any onging composition whenever RWH's Blur() method gets
  // called, because in this case, webkit will confirm the ongoing composition
  // internally.
  [self cancelComposition];

  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];

  // For now, these actions are always enabled for render view,
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(undo:) ||
      action == @selector(redo:) ||
      action == @selector(cut:) ||
      action == @selector(copy:) ||
      action == @selector(copyToFindPboard:) ||
      action == @selector(paste:) ||
      action == @selector(pasteAsPlainText:)) {
    return renderWidgetHostView_->render_widget_host_->IsRenderView();
  }

  return editCommand_helper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return renderWidgetHostView_.get();
}

// Determine whether we should autohide the cursor (i.e., hide it until mouse
// move) for the given event. Customize here to be more selective about which
// key presses to autohide on.
+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event {
  return ([event type] == NSKeyDown) ? YES : NO;
}

// Create the BrowserAccessibility tree from the WebAccessibility tree passed
// from the renderer.
- (void)setAccessibilityTree:(const webkit_glue::WebAccessibility&) tree {
  BrowserAccessibility* root =
      [[BrowserAccessibility alloc] initWithObject:tree
                                          delegate:self
                                            parent:self];
  [root autorelease];
  accessibilityChildren_.reset([[NSArray alloc] initWithObjects:root, nil]);
  accessibilityReceived_ = YES;
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute
                                         index:(NSUInteger)index
                                      maxCount:(NSUInteger)maxCount {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  NSUInteger totalLength = [fullArray count];
  if (index >= totalLength)
    return nil;
  NSUInteger length = MIN(totalLength - index, maxCount);
  return [fullArray subarrayWithRange:NSMakeRange(index, length)];
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
  if (!accessibilityRequested_) {
    renderWidgetHostView_->render_widget_host_->EnableRendererAccessibility();
    accessibilityRequested_ = YES;
  }
  if (accessibilityReceived_) {
    if (rendererAccessible_ &&
        [attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
      return accessibilityChildren_.get();
    } else if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
      return NSAccessibilityScrollAreaRole;
    }
  }
  id ret = [super accessibilityAttributeValue:attribute];
  return ret;
}

- (id)accessibilityHitTest:(NSPoint)point {
  if (!accessibilityRequested_) {
    renderWidgetHostView_->render_widget_host_->EnableRendererAccessibility();
    accessibilityRequested_ = YES;
  }
  if (!accessibilityReceived_) {
    return self;
  }
  NSPoint pointInWindow = [[self window] convertScreenToBase:point];
  NSPoint localPoint = [self convertPoint:pointInWindow fromView:nil];
  localPoint.y = NSHeight([self bounds]) - localPoint.y;
  if ([accessibilityChildren_ count] == 0)
    return self;
  BrowserAccessibility* root = [accessibilityChildren_ objectAtIndex:0];
  id obj = [root accessibilityHitTest:localPoint];
  return obj;
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSUInteger)accessibilityIndexOfChild:(id)child {
  return [accessibilityChildren_ indexOfObject:child];
}

- (void)doDefaultAction:(int32)accessibilityObjectId {
  renderWidgetHostView_->render_widget_host_->
      AccessibilityDoDefaultAction(accessibilityObjectId);
}

// Convert a web accessibility's location in web coordinates into a cocoa
// screen coordinate.
- (NSPoint)accessibilityPointInScreen:(BrowserAccessibility*)accessibility {
  NSPoint origin = [accessibility origin];
  NSSize size = [accessibility size];
  origin.y = NSHeight([self bounds]) - origin.y;
  NSPoint originInWindow = [self convertPoint:origin toView:nil];
  NSPoint originInScreen = [[self window] convertBaseToScreen:originInWindow];
  originInScreen.y = originInScreen.y - size.height;
  return originInScreen;
}

- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId {
  if (focus) {
    renderWidgetHostView_->render_widget_host_->
        SetAccessibilityFocus(accessibilityObjectId);
  }
}

// Spellchecking methods
// The next three methods are implemented here since this class is the first
// responder for anything in the browser.

// This message is sent whenever the user specifies that a word should be
// changed from the spellChecker.
- (void)changeSpelling:(id)sender {
  // Grab the currently selected word from the spell panel, as this is the word
  // that we want to replace the selected word in the text with.
  NSString* newWord = [[sender selectedCell] stringValue];
  if (newWord != nil) {
    RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
    thisHostView->GetRenderWidgetHost()->Replace(
        base::SysNSStringToUTF16(newWord));
  }
}

// This message is sent by NSSpellChecker whenever the next word should be
// advanced to, either after a correction or clicking the "Find Next" button.
// This isn't documented anywhere useful, like in NSSpellProtocol.h with the
// other spelling panel methods. This is probably because Apple assumes that the
// the spelling panel will be used with an NSText, which will automatically
// catch this and advance to the next word for you. Thanks Apple.
- (void)checkSpelling:(id)sender {
  RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
  thisHostView->GetRenderWidgetHost()->AdvanceToNextMisspelling();
}

// This message is sent by the spelling panel whenever a word is ignored.
- (void)ignoreSpelling:(id)sender {
  // Ideally, we would ask the current RenderView for its tag, but that would
  // mean making a blocking IPC call from the browser. Instead,
  // SpellCheckerPlatform::CheckSpelling remembers the last tag and
  // SpellCheckerPlatform::IgnoreWord assumes that is the correct tag.
  NSString* wordToIgnore = [sender stringValue];
  if (wordToIgnore != nil) {
    SpellCheckerPlatform::IgnoreWord(base::SysNSStringToUTF16(wordToIgnore));

    // Strangely, the spellingPanel doesn't send checkSpelling after a word is
    // ignored, so we have to explicitly call AdvanceToNextMisspelling here.
    RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
    thisHostView->GetRenderWidgetHost()->AdvanceToNextMisspelling();
  }
}

- (void)showGuessPanel:(id)sender {
  RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
  thisHostView->GetRenderWidgetHost()->ToggleSpellPanel(
      SpellCheckerPlatform::SpellingPanelVisible());
}

// END Spellchecking methods

// Below is the nasty tooltip stuff -- copied from WebKit's WebHTMLView.mm
// with minor modifications for code style and commenting.
//
//  The 'public' interface is -setToolTipAtMousePoint:. This differs from
// -setToolTip: in that the updated tooltip takes effect immediately,
//  without the user's having to move the mouse out of and back into the view.
//
// Unfortunately, doing this requires sending fake mouseEnter/Exit events to
// the view, which in turn requires overriding some internal tracking-rect
// methods (to keep track of its owner & userdata, which need to be filled out
// in the fake events.) --snej 7/6/09


/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *           (C) 2006, 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Any non-zero value will do, but using something recognizable might help us
// debug some day.
static const NSTrackingRectTag kTrackingRectTag = 0xBADFACE;

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (NSTrackingRectTag)addTrackingRect:(NSRect)rect
                               owner:(id)owner
                            userData:(void *)data
                        assumeInside:(BOOL)assumeInside {
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect
                                owner:(id)owner
                             userData:(void *)data
                         assumeInside:(BOOL)assumeInside
                       useTrackingNum:(int)tag {
  DCHECK(tag == 0 || tag == kTrackingRectTag);
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (void)_addTrackingRects:(NSRect *)rects
                    owner:(id)owner
             userDataList:(void **)userDataList
         assumeInsideList:(BOOL *)assumeInsideList
             trackingNums:(NSTrackingRectTag *)trackingNums
                    count:(int)count {
  DCHECK(count == 1);
  DCHECK(trackingNums[0] == 0 || trackingNums[0] == kTrackingRectTag);
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = userDataList[0];
  trackingNums[0] = kTrackingRectTag;
}

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (void)removeTrackingRect:(NSTrackingRectTag)tag {
  if (tag == 0)
    return;

  if (tag == kTrackingRectTag) {
    trackingRectOwner_ = nil;
    return;
  }

  if (tag == lastToolTipTag_) {
    [super removeTrackingRect:tag];
    lastToolTipTag_ = 0;
    return;
  }

  // If any other tracking rect is being removed, we don't know how it was
  // created and it's possible there's a leak involved (see Radar 3500217).
  NOTREACHED();
}

// Override of (apparently) a private NSView method(!)
- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count {
  for (int i = 0; i < count; ++i) {
    int tag = tags[i];
    if (tag == 0)
      continue;
    DCHECK(tag == kTrackingRectTag);
    trackingRectOwner_ = nil;
  }
}

// Sends a fake NSMouseExited event to the view for its current tracking rect.
- (void)_sendToolTipMouseExited {
  // Nothing matters except window, trackingNumber, and userData.
  int windowNumber = [[self window] windowNumber];
  NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
                                              location:NSMakePoint(0, 0)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:windowNumber
                                               context:NULL
                                           eventNumber:0
                                        trackingNumber:kTrackingRectTag
                                              userData:trackingRectUserData_];
  [trackingRectOwner_ mouseExited:fakeEvent];
}

// Sends a fake NSMouseEntered event to the view for its current tracking rect.
- (void)_sendToolTipMouseEntered {
  // Nothing matters except window, trackingNumber, and userData.
  int windowNumber = [[self window] windowNumber];
  NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
                                              location:NSMakePoint(0, 0)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:windowNumber
                                               context:NULL
                                           eventNumber:0
                                        trackingNumber:kTrackingRectTag
                                              userData:trackingRectUserData_];
  [trackingRectOwner_ mouseEntered:fakeEvent];
}

// Sets the view's current tooltip, to be displayed at the current mouse
// location. (This does not make the tooltip appear -- as usual, it only
// appears after a delay.) Pass null to remove the tooltip.
- (void)setToolTipAtMousePoint:(NSString *)string {
  NSString *toolTip = [string length] == 0 ? nil : string;
  if ((toolTip && toolTip_ && [toolTip isEqualToString:toolTip_]) ||
      (!toolTip && !toolTip_)) {
    return;
  }

  if (toolTip_) {
    [self _sendToolTipMouseExited];
  }

  toolTip_.reset([toolTip copy]);

  if (toolTip) {
    // See radar 3500217 for why we remove all tooltips
    // rather than just the single one we created.
    [self removeAllToolTips];
    NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
    lastToolTipTag_ = [self addToolTipRect:wideOpenRect
                                     owner:self
                                  userData:NULL];
    [self _sendToolTipMouseEntered];
  }
}

// NSView calls this to get the text when displaying the tooltip.
- (NSString *)view:(NSView *)view
  stringForToolTip:(NSToolTipTag)tag
             point:(NSPoint)point
          userData:(void *)data {
  return [[toolTip_ copy] autorelease];
}

// Below is our NSTextInput implementation.
//
// When WebHTMLView receives a NSKeyDown event, WebHTMLView calls the following
// functions to process this event.
//
// [WebHTMLView keyDown] ->
//     EventHandler::keyEvent() ->
//     ...
//     [WebEditorClient handleKeyboardEvent] ->
//     [WebHTMLView _interceptEditingKeyEvent] ->
//     [NSResponder interpretKeyEvents] ->
//     [WebHTMLView insertText] ->
//     Editor::insertText()
//
// Unfortunately, it is hard for Chromium to use this implementation because
// it causes key-typing jank.
// RenderWidgetHostViewMac is running in a browser process. On the other
// hand, Editor and EventHandler are running in a renderer process.
// So, if we used this implementation, a NSKeyDown event is dispatched to
// the following functions of Chromium.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Sync IPC (KeyDown)| (*1) ->
//     EventHandler::keyEvent() (renderer) ->
//     ...
//     EditorClientImpl::handleKeyboardEvent() (renderer) ->
//     |Sync IPC| (*2) ->
//     [RenderWidgetHostViewMac _interceptEditingKeyEvent] (browser) ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC| ->
//     Editor::insertText() (renderer)
//
// (*1) we need to wait until this call finishes since WebHTMLView uses the
// result of EventHandler::keyEvent().
// (*2) we need to wait until this call finishes since WebEditorClient uses
// the result of [WebHTMLView _interceptEditingKeyEvent].
//
// This needs many sync IPC messages sent between a browser and a renderer for
// each key event, which would probably result in key-typing jank.
// To avoid this problem, this implementation processes key events (and input
// method events) totally in a browser process and sends asynchronous input
// events, almost same as KeyboardEvents (and TextEvents) of DOM Level 3, to a
// renderer process.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Async IPC (RawKeyDown)| ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC (Char)| ->
//     Editor::insertText() (renderer)
//
// Since this implementation doesn't have to wait any IPC calls, this doesn't
// make any key-typing jank. --hbono 7/23/09
//
extern "C" {
extern NSString *NSTextInputReplacementRangeAttributeName;
}

- (NSArray *)validAttributesForMarkedText {
  // This code is just copied from WebKit except renaming variables.
  if (!validAttributesForMarkedText_) {
    validAttributesForMarkedText_.reset([[NSArray alloc] initWithObjects:
        NSUnderlineStyleAttributeName,
        NSUnderlineColorAttributeName,
        NSMarkedClauseSegmentAttributeName,
        NSTextInputReplacementRangeAttributeName,
        nil]);
  }
  return validAttributesForMarkedText_.get();
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  NOTIMPLEMENTED();
  return NSNotFound;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange {
  // An input method requests a cursor rectangle to display its candidate
  // window.
  // Calculate the screen coordinate of the cursor rectangle saved in
  // RenderWidgetHostViewMac::ImeUpdateTextInputState() and send it to the
  // input method.
  // Since this window may be moved since we receive the cursor rectangle last
  // time we sent the cursor rectangle to the input method, so we should map
  // from the view coordinate to the screen coordinate every time when an input
  // method need it.
  NSRect resultRect = [self convertRect:caretRect_ toView:nil];
  NSWindow* window = [self window];
  if (window)
    resultRect.origin = [window convertBaseToScreen:resultRect.origin];

  return resultRect;
}

- (NSRange)selectedRange {
  // Return the selected range saved in the setMarkedText method.
  return hasMarkedText_ ? selectedRange_ : NSMakeRange(NSNotFound, 0);
}

- (NSRange)markedRange {
  // An input method calls this method to check if an application really has
  // a text being composed when hasMarkedText call returns true.
  // Returns the range saved in the setMarkedText method so the input method
  // calls the setMarkedText method and we can update the composition node
  // there. (When this method returns an empty range, the input method doesn't
  // call the setMarkedText method.)
  return hasMarkedText_ ? markedRange_ : NSMakeRange(NSNotFound, 0);
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)range {
  // TODO(hbono): Even though many input method works without implementing
  // this method, we need to save a copy of the string in the setMarkedText
  // method and create a NSAttributedString with the given range.
  // http://crbug.com/37715
  return nil;
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

// Each RenderWidgetHostViewCocoa has its own input context, but we return
// nil when the caret is in non-editable content or password box to avoid
// making input methods do their work.
- (NSTextInputContext *)inputContext {
  switch(renderWidgetHostView_->text_input_type_) {
    case WebKit::WebTextInputTypeNone:
    case WebKit::WebTextInputTypePassword:
      return nil;
    default:
      return [super inputContext];
  }
}

- (BOOL)hasMarkedText {
  // An input method calls this function to figure out whether or not an
  // application is really composing a text. If it is composing, it calls
  // the markedRange method, and maybe calls the setMarkedText method.
  // It seems an input method usually calls this function when it is about to
  // cancel an ongoing composition. If an application has a non-empty marked
  // range, it calls the setMarkedText method to delete the range.
  return hasMarkedText_;
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  hasMarkedText_ = NO;
  markedText_.clear();
  underlines_.clear();

  // If we are handling a key down event, then ConfirmComposition() will be
  // called in keyEvent: method.
  if (!handlingKeyDown_)
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition();
  else
    unmarkTextCalled_ = YES;
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];

  markedRange_ = NSMakeRange(0, length);
  selectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);

  underlines_.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &underlines_);
  } else {
    // Use a thin black underline by default.
    underlines_.push_back(
        WebKit::WebCompositionUnderline(0, length, SK_ColorBLACK, false));
  }

  // If we are handling a key down event, then SetComposition() will be
  // called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend can
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if (!handlingKeyDown_) {
    renderWidgetHostView_->render_widget_host_->ImeSetComposition(
        markedText_, underlines_,
        newSelRange.location, NSMaxRange(newSelRange));
  }
}

- (void)doCommandBySelector:(SEL)selector {
  // An input method calls this function to dispatch an editing command to be
  // handled by this view.
  if (selector == @selector(noop:))
    return;

  std::string command(
      [RWHVMEditCommandHelper::CommandNameForSelector(selector) UTF8String]);

  // If this method is called when handling a key down event, then we need to
  // handle the command in the key event handler. Otherwise we can just handle
  // it here.
  if (handlingKeyDown_) {
    hasEditCommands_ = YES;
    // We ignore commands that insert characters, because this was causing
    // strange behavior (e.g. tab always inserted a tab rather than moving to
    // the next field on the page).
    if (!StartsWithASCII(command, "insert", false))
      editCommands_.push_back(EditCommand(command, ""));
  } else {
    renderWidgetHostView_->render_widget_host_->ForwardEditCommand(command, "");
  }
}

- (void)insertText:(id)string {
  // An input method has characters to be inserted.
  // Same as Linux, Mac calls this method not only:
  // * when an input method finishs composing text, but also;
  // * when we type an ASCII character (without using input methods).
  // When we aren't using input methods, we should send the given character as
  // a Char event so it is dispatched to an onkeypress() event handler of
  // JavaScript.
  // On the other hand, when we are using input methods, we should send the
  // given characters as an input method event and prevent the characters from
  // being dispatched to onkeypress() event handlers.
  // Text inserting might be initiated by other source instead of keyboard
  // events, such as the Characters dialog. In this case the text should be
  // sent as an input method event as well.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
  } else {
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::SysNSStringToUTF16(im_text));
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

- (void)viewDidMoveToWindow {
  if (canBeKeyView_) {
    NSWindow* newWindow = [self window];
    // Pointer comparison only, since we don't know if lastWindow_ is still
    // valid.
    if (newWindow) {
      // If we move into a new window, refresh the frame information. We
      // don't need to do it if it was the same window as it used to be in,
      // since that case is covered by DidBecomeSelected. We only want to
      // do this for real browser views, not popups.
      if (newWindow != lastWindow_) {
        lastWindow_ = newWindow;
        renderWidgetHostView_->WindowFrameChanged();
      }
      renderWidgetHostView_->ForceTextureReload();
    }
  }

  // If we switch windows (or are removed from the view hierarchy), cancel any
  // open mouse-downs.
  if (hasOpenMouseDown_) {
    WebMouseEvent event;
    event.type = WebInputEvent::MouseUp;
    event.button = WebMouseEvent::ButtonLeft;
    if (renderWidgetHostView_->render_widget_host_)
      renderWidgetHostView_->render_widget_host_->ForwardMouseEvent(event);

    hasOpenMouseDown_ = NO;
  }
}

- (void)undo:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      Undo();
  }
}

- (void)redo:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      Redo();
  }
}

- (void)cut:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      Cut();
  }
}

- (void)copy:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      Copy();
  }
}

- (void)copyToFindPboard:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      CopyToFindPboard();
  }
}

- (void)paste:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      Paste();
  }
}

- (void)pasteAsPlainText:(id)sender {
  if (renderWidgetHostView_->render_widget_host_->IsRenderView()) {
    static_cast<RenderViewHost*>(renderWidgetHostView_->render_widget_host_)->
      ForwardEditCommand("PasteAndMatchStyle", "");
  }
}

- (void)cancelComposition {
  if (!hasMarkedText_)
    return;

  // Cancel the ongoing composition. [NSInputManager markedTextAbandoned:]
  // doesn't call any NSTextInput functions, such as setMarkedText or
  // insertText. So, we need to send an IPC message to a renderer so it can
  // delete the composition node.
  NSInputManager *currentInputManager = [NSInputManager currentInputManager];
  [currentInputManager markedTextAbandoned:self];

  hasMarkedText_ = NO;
  // Should not call [self unmarkText] here, because it'll send unnecessary
  // cancel composition IPC message to the renderer.
}

- (void)confirmComposition {
  if (!hasMarkedText_)
    return;

  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition();

  [self cancelComposition];
}

- (ViewID)viewID {
  return VIEW_ID_TAB_CONTAINER_FOCUS_VIEW;
}

// Overriding a NSResponder method to support application services.

- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  id requestor = nil;
  BOOL sendTypeIsString = [sendType isEqual:NSStringPboardType];
  BOOL returnTypeIsString = [returnType isEqual:NSStringPboardType];
  BOOL hasText = !renderWidgetHostView_->selected_text().empty();
  BOOL takesText =
      renderWidgetHostView_->text_input_type_ != WebKit::WebTextInputTypeNone;

  if (sendTypeIsString && hasText && !returnType) {
    requestor = self;
  } else if (!sendType && returnTypeIsString && takesText) {
    requestor = self;
  } else if (sendTypeIsString && returnTypeIsString && hasText && takesText) {
    requestor = self;
  } else {
    requestor = [super validRequestorForSendType:sendType
                                      returnType:returnType];
  }
  return requestor;
}

@end

//
// Supporting application services
//
@implementation RenderWidgetHostViewCocoa(NSServicesRequests)

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard
                             types:(NSArray*)types {
  const std::string& str = renderWidgetHostView_->selected_text();
  if (![types containsObject:NSStringPboardType] || str.empty()) return NO;

  scoped_nsobject<NSString> text([[NSString alloc]
                                   initWithUTF8String:str.c_str()]);
  NSArray* toDeclare = [NSArray arrayWithObject:NSStringPboardType];
  [pboard declareTypes:toDeclare owner:nil];
  return [pboard setString:text forType:NSStringPboardType];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSString *string = [pboard stringForType:NSStringPboardType];
  if (!string) return NO;

  // If the user is currently using an IME, confirm the IME input,
  // and then insert the text from the service, the same as TextEdit and Safari.
  [self confirmComposition];
  [self insertText:string];
  return YES;
}

@end
