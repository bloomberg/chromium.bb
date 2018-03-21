// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#import <Carbon/Carbon.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/renderer_host/cursor_manager.h"
#import "content/browser/renderer_host/input/synthetic_gesture_target_mac.h"
#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#import "content/browser/renderer_host/render_widget_host_view_cocoa.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#import "content/browser/renderer_host/text_input_client_mac.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/fullscreen_window_manager.h"
#import "ui/base/cocoa/secure_password_input.h"
#include "ui/base/cocoa/text_services_context_menu.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/gl_switches.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebGestureEvent;

// A window subclass that allows the fullscreen window to become main and gain
// keyboard focus. This is only used for pepper flash. Normal fullscreen is
// handled by the browser.
@interface PepperFlashFullscreenWindow : NSWindow
@end

@implementation PepperFlashFullscreenWindow

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return YES;
}

@end

@interface RenderWidgetPopupWindow : NSWindow {
   // The event tap that allows monitoring of all events, to properly close with
   // a click outside the bounds of the window.
  id clickEventTap_;
}
@end

@implementation RenderWidgetPopupWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  if (self = [super initWithContentRect:contentRect
                              styleMask:windowStyle
                                backing:bufferingType
                                  defer:deferCreation]) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self startObservingClicks];
  }
  return self;
}

- (void)close {
  [self stopObservingClicks];
  [super close];
}

// Gets called when the menubar is clicked.
// Needed because the local event monitor doesn't see the click on the menubar.
- (void)beganTracking:(NSNotification*)notification {
  [self close];
}

// Install the callback.
- (void)startObservingClicks {
  clickEventTap_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask
      handler:^NSEvent* (NSEvent* event) {
          if ([event window] == self)
            return event;
          NSEventType eventType = [event type];
          if (eventType == NSLeftMouseDown || eventType == NSRightMouseDown)
            [self close];
          return event;
  }];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
         selector:@selector(beganTracking:)
             name:NSMenuDidBeginTrackingNotification
           object:[NSApp mainMenu]];
}

// Remove the callback.
- (void)stopObservingClicks {
  if (!clickEventTap_)
    return;

  [NSEvent removeMonitor:clickEventTap_];
   clickEventTap_ = nil;

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter removeObserver:self
                name:NSMenuDidBeginTrackingNotification
              object:[NSApp mainMenu]];
}

@end

namespace {

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

float FlipYFromRectToScreen(float y, float rect_height) {
  TRACE_EVENT0("browser", "FlipYFromRectToScreen");
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetPrimaryDisplay();
  CGFloat screen_zero_height = display.bounds().height();
  if (screen_zero_height == 0.f)
    return y;
  return screen_zero_height - y - rect_height;
}

// Adjusts an NSRect in Cocoa screen coordinates to have an origin in the upper
// left of the primary screen (Carbon coordinates), and stuffs it into a
// gfx::Rect.
gfx::Rect FlipNSRectToRectScreen(const NSRect& rect) {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y(FlipYFromRectToScreen(new_rect.y(), new_rect.height()));
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

namespace content {

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorMacClient, public:

SkColor RenderWidgetHostViewMac::BrowserCompositorMacGetGutterColor() const {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (host()->delegate() && host()->delegate()->IsFullscreenForCurrentTab()) {
    return SK_ColorBLACK;
  }
  return last_frame_root_background_color_;
}

void RenderWidgetHostViewMac::BrowserCompositorMacOnBeginFrame() {
  UpdateNeedsBeginFramesInternal();
}

void RenderWidgetHostViewMac::OnFrameTokenChanged(uint32_t frame_token) {
  OnFrameTokenChangedForView(frame_token);
}

void RenderWidgetHostViewMac::DidReceiveFirstFrameAfterNavigation() {
  host()->DidReceiveFirstFrameAfterNavigation();
}

void RenderWidgetHostViewMac::DestroyCompositorForShutdown() {
  browser_compositor_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMacNSView, public:

NSView* RenderWidgetHostViewMac::AcceleratedWidgetGetNSView() const {
  return cocoa_view_;
}

void RenderWidgetHostViewMac::AcceleratedWidgetGetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) const {
  if (display_link_ &&
      display_link_->GetVSyncParameters(timebase, interval))
    return;
  *timebase = base::TimeTicks();
  *interval = base::TimeDelta();
}

void RenderWidgetHostViewMac::AcceleratedWidgetSwapCompleted() {
  // Set the background color for the root layer from the frame that just
  // swapped. See RenderWidgetHostViewAura for more details. Note that this is
  // done only after the swap has completed, so that the background is not set
  // before the frame is up.
  SetBackgroundLayerColor(last_frame_root_background_color_);

  if (display_link_)
    display_link_->NotifyCurrentTime(base::TimeTicks::Now());
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget,
                                                 bool is_guest_view_hack)
    : RenderWidgetHostViewBase(widget),
      page_at_minimum_scale_(true),
      mouse_wheel_phase_handler_(this),
      is_loading_(false),
      allow_pause_for_resize_or_repaint_(true),
      is_guest_view_hack_(is_guest_view_hack),
      fullscreen_parent_host_view_(nullptr),
      weak_factory_(this) {
  // |cocoa_view_| owns us and we will be deleted when |cocoa_view_|
  // goes away.  Since we autorelease it, our caller must put
  // |GetNativeView()| into the view hierarchy right after calling us.
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];

  background_layer_.reset([[CALayer alloc] init]);
  [cocoa_view_ setLayer:background_layer_];
  [cocoa_view_ setWantsLayer:YES];

  viz::FrameSinkId frame_sink_id = is_guest_view_hack_
                                       ? AllocateFrameSinkIdForGuestViewHack()
                                       : host()->GetFrameSinkId();

  browser_compositor_.reset(new BrowserCompositorMac(
      this, this, host()->is_hidden(), [cocoa_view_ window], frame_sink_id));

  display::Screen::GetScreen()->AddObserver(this);

  if (!is_guest_view_hack_)
    host()->SetView(this);

  // Let the page-level input event router know about our surface ID
  // namespace for surface-based hit testing.
  if (host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

  RenderViewHost* rvh = RenderViewHost::From(host());
  bool needs_begin_frames = true;

  if (rvh) {
    // TODO(mostynb): actually use prefs.  Landing this as a separate CL
    // first to rebaseline some unreliable layout tests.
    ignore_result(rvh->GetWebkitPreferences());
    needs_begin_frames = !rvh->GetDelegate()->IsNeverVisible();
  }

  cursor_manager_.reset(new CursorManager(this));

  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);

  // Because of the way Mac pumps messages during resize, SetNeedsBeginFrame
  // messages are not delayed on Mac.  This leads to creation-time raciness
  // where renderer sends a SetNeedsBeginFrame(true) before the renderer host is
  // created to receive it.
  //
  // Any renderer that will produce frames needs to have begin frames sent to
  // it. So unless it is never visible, start this value at true here to avoid
  // startup raciness and decrease latency.
  needs_begin_frames_ = needs_begin_frames;
  UpdateNeedsBeginFramesInternal();
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
  display::Screen::GetScreen()->RemoveObserver(this);

  // This is being called from |cocoa_view_|'s destructor, so invalidate the
  // pointer.
  cocoa_view_ = nil;

  UnlockMouse();

  browser_compositor_.reset();

  // We are owned by RenderWidgetHostViewCocoa, so if we go away before the
  // RenderWidgetHost does we need to tell it not to hold a stale pointer to
  // us.
  if (host()) {
    // If this is a RenderWidgetHostViewGuest's platform_view_, we're not the
    // RWH's view, the RenderWidgetHostViewGuest is. So don't reset the RWH's
    // view, the RenderWidgetHostViewGuest will do it.
    if (!is_guest_view_hack_)
      host()->SetView(NULL);
  }

  // In case the view is deleted (by cocoa view) before calling destroy, we need
  // to remove this view from the observer list of TextInputManager.
  if (text_input_manager_ && text_input_manager_->HasObserver(this))
    text_input_manager_->RemoveObserver(this);
}

void RenderWidgetHostViewMac::SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate) {
  [cocoa_view_ setResponderDelegate:delegate];
}

void RenderWidgetHostViewMac::SetAllowPauseForResizeOrRepaint(bool allow) {
  allow_pause_for_resize_or_repaint_ = allow;
}

ui::TextInputType RenderWidgetHostViewMac::GetTextInputType() {
  if (!GetActiveWidget())
    return ui::TEXT_INPUT_TYPE_NONE;
  return text_input_manager_->GetTextInputState()->type;
}

RenderWidgetHostImpl* RenderWidgetHostViewMac::GetActiveWidget() {
  return text_input_manager_ ? text_input_manager_->GetActiveWidget() : nullptr;
}

const TextInputManager::CompositionRangeInfo*
RenderWidgetHostViewMac::GetCompositionRangeInfo() {
  return text_input_manager_ ? text_input_manager_->GetCompositionRangeInfo()
                             : nullptr;
}

const TextInputManager::TextSelection*
RenderWidgetHostViewMac::GetTextSelection() {
  return text_input_manager_ ? text_input_manager_->GetTextSelection(
                                   GetFocusedViewForTextSelection())
                             : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::InitAsChild(
    gfx::NativeView parent_view) {
}

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  bool activatable = popup_type_ == blink::kWebPopupTypeNone;
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:activatable ? YES : NO];

  NSPoint origin_global = NSPointFromCGPoint(pos.origin().ToCGPoint());
  origin_global.y = FlipYFromRectToScreen(origin_global.y, pos.height());

  popup_window_.reset([[RenderWidgetPopupWindow alloc]
      initWithContentRect:NSMakeRect(origin_global.x, origin_global.y,
                                     pos.width(), pos.height())
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [popup_window_ setLevel:NSPopUpMenuWindowLevel];
  [popup_window_ setReleasedWhenClosed:NO];
  [popup_window_ makeKeyAndOrderFront:nil];
  [[popup_window_ contentView] addSubview:cocoa_view_];
  [cocoa_view_ setFrame:[[popup_window_ contentView] bounds]];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[NSNotificationCenter defaultCenter]
      addObserver:cocoa_view_
         selector:@selector(popupWindowWillClose:)
             name:NSWindowWillCloseNotification
           object:popup_window_];
}

// This function creates the fullscreen window and hides the dock and menubar if
// necessary. Note, this codepath is only used for pepper flash when
// pp::FlashFullScreen::SetFullscreen() is called. If
// pp::FullScreen::SetFullscreen() is called then the entire browser window
// will enter fullscreen instead.
void RenderWidgetHostViewMac::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  fullscreen_parent_host_view_ =
      static_cast<RenderWidgetHostViewMac*>(reference_host_view);
  NSWindow* parent_window = nil;
  if (reference_host_view)
    parent_window = [reference_host_view->GetNativeView() window];
  NSScreen* screen = [parent_window screen];
  if (!screen)
    screen = [NSScreen mainScreen];

  pepper_fullscreen_window_.reset([[PepperFlashFullscreenWindow alloc]
      initWithContentRect:[screen frame]
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [pepper_fullscreen_window_ setLevel:NSFloatingWindowLevel];
  [pepper_fullscreen_window_ setReleasedWhenClosed:NO];
  [cocoa_view_ setCanBeKeyView:YES];
  [cocoa_view_ setFrame:[[pepper_fullscreen_window_ contentView] bounds]];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  // If the pepper fullscreen window isn't opaque then there are performance
  // issues when it's on the discrete GPU and the Chrome window is being drawn
  // to. http://crbug.com/171911
  [pepper_fullscreen_window_ setOpaque:YES];

  // Note that this forms a reference cycle between the fullscreen window and
  // the rwhvmac: The PepperFlashFullscreenWindow retains cocoa_view_,
  // but cocoa_view_ keeps pepper_fullscreen_window_ in an instance variable.
  // This cycle is normally broken when -keyEvent: receives an <esc> key, which
  // explicitly calls Shutdown on the host(), which calls
  // Destroy() on RWHVMac, which drops the reference to
  // pepper_fullscreen_window_.
  [[pepper_fullscreen_window_ contentView] addSubview:cocoa_view_];

  // Note that this keeps another reference to pepper_fullscreen_window_.
  fullscreen_window_manager_.reset([[FullscreenWindowManager alloc]
      initWithWindow:pepper_fullscreen_window_.get()
       desiredScreen:screen]);
  [fullscreen_window_manager_ enterFullscreenMode];
  [pepper_fullscreen_window_ makeKeyAndOrderFront:nil];
}

void RenderWidgetHostViewMac::release_pepper_fullscreen_window_for_testing() {
  // See comment in InitAsFullscreen(): There is a reference cycle between
  // rwhvmac and fullscreen window, which is usually broken by hitting <esc>.
  // Tests that test pepper fullscreen mode without sending an <esc> event
  // need to call this method to break the reference cycle.
  [fullscreen_window_manager_ exitFullscreenMode];
  fullscreen_window_manager_.reset();
  [pepper_fullscreen_window_ close];
  pepper_fullscreen_window_.reset();
}

int RenderWidgetHostViewMac::window_number() const {
  NSWindow* window = [cocoa_view_ window];
  if (!window)
    return -1;
  return [window windowNumber];
}

void RenderWidgetHostViewMac::UpdateDisplayLink() {
  static bool is_vsync_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuVsync);
  if (is_vsync_disabled)
    return;

  NSScreen* screen = [[cocoa_view_ window] screen];
  NSDictionary* screen_description = [screen deviceDescription];
  NSNumber* screen_number = [screen_description objectForKey:@"NSScreenNumber"];
  CGDirectDisplayID display_id = [screen_number unsignedIntValue];

  display_link_ = ui::DisplayLinkMac::GetForDisplay(display_id);
  if (!display_link_.get()) {
    // Note that on some headless systems, the display link will fail to be
    // created, so this should not be a fatal error.
    LOG(ERROR) << "Failed to create display link.";
  }
}

void RenderWidgetHostViewMac::UpdateDisplayVSyncParameters() {
  if (!host() || !display_link_.get())
    return;

  if (!display_link_->GetVSyncParameters(&vsync_timebase_, &vsync_interval_)) {
    vsync_timebase_ = base::TimeTicks();
    vsync_interval_ = base::TimeDelta();
    return;
  }

  browser_compositor_->UpdateVSyncParameters(vsync_timebase_, vsync_interval_);
}

RenderWidgetHostViewBase*
    RenderWidgetHostViewMac::GetFocusedViewForTextSelection() {
  // We obtain the TextSelection from focused RWH which is obtained from the
  // frame tree. BrowserPlugin-based guests' RWH is not part of the frame tree
  // and the focused RWH will be that of the embedder which is incorrect. In
  // this case we should use TextSelection for |this| since RWHV for guest
  // forwards text selection information to its platform view.
  return is_guest_view_hack_ ? this : GetFocusedWidget()
                                          ? GetFocusedWidget()->GetView()
                                          : nullptr;
}

RenderWidgetHostDelegate*
RenderWidgetHostViewMac::GetFocusedRenderWidgetHostDelegate() {
  if (auto* focused_widget = GetFocusedWidget())
    return focused_widget->delegate();
  return host()->delegate();
}

void RenderWidgetHostViewMac::UpdateNSViewAndDisplayProperties() {
  if (!browser_compositor_)
    return;

  // During auto-resize it is the responsibility of the caller to ensure that
  // the NSView and RenderWidgetHostImpl are kept in sync.
  if (host()->auto_resize_enabled())
    return;

  if (host()->delegate())
    host()->delegate()->SendScreenRects();
  else
    host()->SendScreenRects();

  // RenderWidgetHostImpl will query BrowserCompositorMac for the dimensions
  // to send to the renderer, so it is required that BrowserCompositorMac be
  // updated first. Only notify RenderWidgetHostImpl of the update if any
  // properties it will query have changed.
  if (browser_compositor_->UpdateNSViewAndDisplay())
    host()->NotifyScreenInfoChanged();
}

void RenderWidgetHostViewMac::GetScreenInfo(ScreenInfo* screen_info) const {
  browser_compositor_->GetRendererScreenInfo(screen_info);
}

void RenderWidgetHostViewMac::Show() {
  ScopedCAActionDisabler disabler;
  [cocoa_view_ setHidden:NO];

  browser_compositor_->SetRenderWidgetHostIsHidden(false);

  ui::LatencyInfo renderer_latency_info;
  renderer_latency_info.AddLatencyNumber(ui::TAB_SHOW_COMPONENT,
                                         host()->GetLatencyComponentId(), 0);
  renderer_latency_info.set_trace_id(++tab_show_sequence_);
  host()->WasShown(renderer_latency_info);
  TRACE_EVENT_ASYNC_BEGIN0("latency", "TabSwitching::Latency",
                           tab_show_sequence_);

  // If there is not a frame being currently drawn, kick one, so that the below
  // pause will have a frame to wait on.
  host()->ScheduleComposite();
  PauseForPendingResizeOrRepaintsAndDraw();
}

void RenderWidgetHostViewMac::Hide() {
  if (!browser_compositor_)
    return;

  ScopedCAActionDisabler disabler;
  [cocoa_view_ setHidden:YES];

  host()->WasHidden();
  browser_compositor_->SetRenderWidgetHostIsHidden(true);
}

void RenderWidgetHostViewMac::WasUnOccluded() {
  if (!browser_compositor_)
    return;

  browser_compositor_->SetRenderWidgetHostIsHidden(false);
  host()->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewMac::WasOccluded() {
  if (!browser_compositor_)
    return;

  host()->WasHidden();
  browser_compositor_->SetRenderWidgetHostIsHidden(true);
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  gfx::Rect rect = GetViewBounds();
  rect.set_size(size);
  SetBounds(rect);
}

void RenderWidgetHostViewMac::SetBounds(const gfx::Rect& rect) {
  // |rect.size()| is view coordinates, |rect.origin| is screen coordinates,
  // TODO(thakis): fix, http://crbug.com/73362

  // During the initial creation of the RenderWidgetHostView in
  // WebContentsImpl::CreateRenderViewForRenderManager, SetSize is called with
  // an empty size. In the Windows code flow, it is not ignored because
  // subsequent sizing calls from the OS flow through TCVW::WasSized which calls
  // SetSize() again. On Cocoa, we rely on the Cocoa view struture and resizer
  // flags to keep things sized properly. On the other hand, if the size is not
  // empty then this is a valid request for a pop-up.
  if (rect.size().IsEmpty())
    return;

  // Ignore the position of |rect| for non-popup rwhvs. This is because
  // background tabs do not have a window, but the window is required for the
  // coordinate conversions. Popups are always for a visible tab.
  //
  // Note: If |cocoa_view_| has been removed from the view hierarchy, it's still
  // valid for resizing to be requested (e.g., during tab capture, to size the
  // view to screen-capture resolution). In this case, simply treat the view as
  // relative to the screen.
  BOOL isRelativeToScreen = IsPopup() ||
      ![[cocoa_view_ superview] isKindOfClass:[BaseView class]];
  if (isRelativeToScreen) {
    // The position of |rect| is screen coordinate system and we have to
    // consider Cocoa coordinate system is upside-down and also multi-screen.
    NSPoint origin_global = NSPointFromCGPoint(rect.origin().ToCGPoint());
    NSSize size = NSMakeSize(rect.width(), rect.height());
    size = [cocoa_view_ convertSize:size toView:nil];
    origin_global.y = FlipYFromRectToScreen(origin_global.y, size.height);
    NSRect frame = NSMakeRect(origin_global.x, origin_global.y,
                              size.width, size.height);
    if (IsPopup())
      [popup_window_ setFrame:frame display:YES];
    else
      [cocoa_view_ setFrame:frame];
  } else {
    BaseView* superview = static_cast<BaseView*>([cocoa_view_ superview]);
    gfx::Rect rect2 = [superview flipNSRectToRect:[cocoa_view_ frame]];
    rect2.set_width(rect.width());
    rect2.set_height(rect.height());
    [cocoa_view_ setFrame:[superview flipRectToNSRect:rect2]];
  }
}

gfx::Vector2dF RenderWidgetHostViewMac::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() const {
  return cocoa_view_;
}

gfx::NativeViewAccessible RenderWidgetHostViewMac::GetNativeViewAccessible() {
  return cocoa_view_;
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

bool RenderWidgetHostViewMac::HasFocus() const {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

bool RenderWidgetHostViewMac::IsSurfaceAvailableForCopy() const {
  return browser_compositor_->GetDelegatedFrameHost()
      ->CanCopyFromCompositingSurface();
}

bool RenderWidgetHostViewMac::IsShowing() {
  return ![cocoa_view_ isHidden];
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() const {
  NSRect bounds = [cocoa_view_ bounds];
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return gfx::Rect(gfx::Size(NSWidth(bounds), NSHeight(bounds)));

  bounds = [cocoa_view_ convertRect:bounds toView:nil];
  bounds = [enclosing_window convertRectToScreen:bounds];
  return FlipNSRectToRectScreen(bounds);
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  GetCursorManager()->UpdateCursor(this, cursor);
}

void RenderWidgetHostViewMac::DisplayCursor(const WebCursor& cursor) {
  WebCursor web_cursor = cursor;
  [cocoa_view_ updateCursor:web_cursor.GetNativeCursor()];
}

CursorManager* RenderWidgetHostViewMac::GetCursorManager() {
  return cursor_manager_.get();
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // If we ever decide to show the waiting cursor while the page is loading
  // like Chrome does on Windows, call |UpdateCursor()| here.
}

void RenderWidgetHostViewMac::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  if (!did_update_state)
    return;

  // |updated_view| is the last view to change its TextInputState which can be
  // used to start/stop monitoring composition info when it has a focused
  // editable text input field.
  RenderWidgetHostImpl* widgetHost =
      RenderWidgetHostImpl::From(updated_view->GetRenderWidgetHost());

  // We might end up here when |updated_view| has had active TextInputState and
  // then got destroyed. In that case, |updated_view->GetRenderWidgetHost()|
  // returns nullptr.
  if (!widgetHost)
    return;

  // Set the monitor state based on the text input focus state.
  const bool has_focus = HasFocus();
  const TextInputState* state = text_input_manager->GetTextInputState();
  bool need_monitor_composition =
      has_focus && state && state->type != ui::TEXT_INPUT_TYPE_NONE;

  widgetHost->RequestCompositionUpdates(false /* immediate_request */,
                                        need_monitor_composition);

  if (has_focus) {
    SetTextInputActive(true);

    // Let AppKit cache the new input context to make IMEs happy.
    // See http://crbug.com/73039.
    [NSApp updateWindows];

#ifndef __LP64__
    bool can_compose_inline =
        !!GetTextInputManager()->GetActiveWidget()
            ? GetTextInputManager()->GetTextInputState()->can_compose_inline
            : true;
    UseInputWindow(TSMGetActiveDocument(), !can_compose_inline);
#endif
  }
}

void RenderWidgetHostViewMac::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  [cocoa_view_ cancelComposition];
}

void RenderWidgetHostViewMac::OnImeCompositionRangeChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  const TextInputManager::CompositionRangeInfo* info =
      GetCompositionRangeInfo();
  if (!info)
    return;
  // The RangeChanged message is only sent with valid values. The current
  // caret position (start == end) will be sent if there is no IME range.
  [cocoa_view_ setMarkedRange:info->range.ToNSRange()];
}

void RenderWidgetHostViewMac::OnSelectionBoundsChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(GetTextInputManager(), text_input_manager);

  // The rest of the code is to support the Mac Zoom feature tracking the
  // text caret; we can skip it if that feature is not currently enabled.
  if (!UAZoomEnabled())
    return;

  RenderWidgetHostViewBase* focused_view = GetFocusedViewForTextSelection();
  if (!focused_view)
    return;

  const TextInputManager::SelectionRegion* region =
      GetTextInputManager()->GetSelectionRegion(focused_view);
  if (!region)
    return;

  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return;

  // Create a rectangle for the edge of the selection focus, which will be
  // the same as the caret position if the selection is collapsed. That's
  // what we want to try to keep centered on-screen if possible.
  gfx::Rect gfx_caret_rect(region->focus.edge_top_rounded().x(),
                           region->focus.edge_top_rounded().y(),
                           1, region->focus.GetHeight());

  // Convert the caret rect to CG-style flipped widget-relative coordinates.
  NSRect caret_rect = NSRectFromCGRect(gfx_caret_rect.ToCGRect());
  caret_rect.origin.y = NSHeight([cocoa_view_ bounds]) -
      (caret_rect.origin.y + caret_rect.size.height);

  // Now convert that to screen coordinates.
  caret_rect = [cocoa_view_ convertRect:caret_rect toView:nil];
  caret_rect = [enclosing_window convertRectToScreen:caret_rect];

  // Finally, flip it again because UAZoomChangeFocus wants unflipped screen
  // coordinates, and call UAZoomChangeFocus to initiate the scroll.
  caret_rect.origin.y = FlipYFromRectToScreen(
      caret_rect.origin.y, caret_rect.size.height);
  UAZoomChangeFocus(&caret_rect, &caret_rect, kUAZoomFocusTypeInsertionPoint);
}

void RenderWidgetHostViewMac::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(GetTextInputManager(), text_input_manager);

  const TextInputManager::TextSelection* selection = GetTextSelection();
  if (!selection)
    return;

  [cocoa_view_ setSelectedRange:selection->range().ToNSRange()];
  // Updates markedRange when there is no marked text so that retrieving
  // markedRange immediately after calling setMarkdText: returns the current
  // caret position.
  if (![cocoa_view_ hasMarkedText]) {
    [cocoa_view_ setMarkedRange:selection->range().ToNSRange()];
  }
}

void RenderWidgetHostViewMac::RenderProcessGone(base::TerminationStatus status,
                                                int error_code) {
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // FrameSinkIds registered with RenderWidgetHostInputEventRouter
  // have already been cleared when RenderWidgetHostViewBase notified its
  // observers of our impending destruction.
  [[NSNotificationCenter defaultCenter]
      removeObserver:cocoa_view_
                name:NSWindowWillCloseNotification
              object:popup_window_];

  // We've been told to destroy.
  [cocoa_view_ retain];
  [cocoa_view_ removeFromSuperview];
  [cocoa_view_ autorelease];

  [popup_window_ close];
  popup_window_.autorelease();

  [fullscreen_window_manager_ exitFullscreenMode];
  fullscreen_window_manager_.reset();
  [pepper_fullscreen_window_ close];

  // This can be called as part of processing the window's responder
  // chain, for instance |-performKeyEquivalent:|.  In that case the
  // object needs to survive until the stack unwinds.
  pepper_fullscreen_window_.autorelease();

  // Delete the delegated frame state, which will reach back into
  // host().
  browser_compositor_.reset();

  // Make sure none of our observers send events for us to process after
  // we release host().
  NotifyObserversAboutShutdown();

  if (text_input_manager_)
    text_input_manager_->RemoveObserver(this);

  mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();

  // We get this call just before host() deletes
  // itself.  But we are owned by |cocoa_view_|, which may be retained
  // by some other code.  Examples are WebContentsViewMac's
  // |latent_focus_view_| and TabWindowController's
  // |cachedContentView_|.
  RenderWidgetHostViewBase::Destroy();
}

// Called from the renderer to tell us what the tooltip text should be. It
// calls us frequently so we need to cache the value to prevent doing a lot
// of repeat work.
void RenderWidgetHostViewMac::SetTooltipText(
    const base::string16& tooltip_text) {
  if (tooltip_text != tooltip_text_ && [[cocoa_view_ window] isKeyWindow]) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite. Don't persist the trimmed
    // string, as then the comparison above will always fail and we'll try to
    // set it again every single time the mouse moves.
    base::string16 display_text = tooltip_text_;
    if (tooltip_text_.length() > kMaxTooltipLength)
      display_text = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysUTF16ToNSString(display_text);
    [cocoa_view_ setToolTipAtMousePoint:tooltip_nsstring];
  }
}

viz::ScopedSurfaceIdAllocator RenderWidgetHostViewMac::ResizeDueToAutoResize(
    const gfx::Size& new_size,
    uint64_t sequence_number) {
  // TODO(cblume): This doesn't currently suppress allocation.
  // It maintains existing behavior while using the suppression style.
  // This will be addressed in a follow-up patch.
  // See https://crbug.com/805073
  base::OnceCallback<void()> allocation_task =
      base::BindOnce(&RenderWidgetHostViewMac::OnResizeDueToAutoResizeComplete,
                     weak_factory_.GetWeakPtr(), new_size, sequence_number);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

void RenderWidgetHostViewMac::DidNavigate() {
  browser_compositor_->DidNavigate();
}

gfx::Size RenderWidgetHostViewMac::GetRequestedRendererSize() const {
  return browser_compositor_->GetRendererSize();
}

namespace {

// A helper function for CombineTextNodesAndMakeCallback() below. It would
// ordinarily be a helper lambda in that class method, but it processes a tree
// and needs to be recursive, and that's crazy difficult to do with a lambda.
// TODO(avi): Move this to be a lambda when P0839R0 lands in C++.
void AddTextNodesToVector(const ui::AXNode* node,
                          std::vector<base::string16>* strings) {
  const ui::AXNodeData& node_data = node->data();

  if (node_data.role == ax::mojom::Role::kStaticText) {
    if (node_data.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      strings->emplace_back(
          node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
    }
    return;
  }

  for (const auto* child : node->children())
    AddTextNodesToVector(child, strings);
}

using SpeechCallback = base::OnceCallback<void(const base::string16&)>;
void CombineTextNodesAndMakeCallback(SpeechCallback callback,
                                     const ui::AXTreeUpdate& update) {
  std::vector<base::string16> text_node_contents;
  text_node_contents.reserve(update.nodes.size());

  ui::AXTree tree(update);

  AddTextNodesToVector(tree.root(), &text_node_contents);

  std::move(callback).Run(
      base::JoinString(text_node_contents, base::ASCIIToUTF16("\n")));
}

}  // namespace

void RenderWidgetHostViewMac::GetPageTextForSpeech(SpeechCallback callback) {
  // Note that the WebContents::RequestAXTreeSnapshot() call has a limit on the
  // number of nodes returned. For large pages, this call might hit that limit.
  // This is a reasonable thing. The "Start Speaking" call dates back to the
  // earliest days of the Mac, before accessibility. It was designed to show off
  // the speech capabilities of the Mac, which is fine, but is mostly
  // inapplicable nowadays. Is it useful to have the Mac read megabytes of text
  // with zero control over positioning, with no fast-forward or rewind? What
  // does it even mean to read a Web 2.0 dynamic, AJAXy page aloud from
  // beginning to end?
  //
  // If this is an issue, please file a bug explaining the situation and how the
  // limits of this feature affect you in the real world.

  GetWebContents()->RequestAXTreeSnapshot(
      base::BindOnce(CombineTextNodesAndMakeCallback, std::move(callback)),
      ui::AXMode::kWebContents);
}

void RenderWidgetHostViewMac::SpeakSelection() {
  const TextInputManager::TextSelection* selection = GetTextSelection();
  if (selection && !selection->selected_text().empty()) {
    ui::TextServicesContextMenu::SpeakText(selection->selected_text());
    return;
  }

  // With no selection, speak an approximation of the entire contents of the
  // page.
  GetPageTextForSpeech(base::BindOnce(ui::TextServicesContextMenu::SpeakText));
}

//
// RenderWidgetHostViewCocoa uses the stored selection text,
// which implements NSServicesRequests protocol.
//

void RenderWidgetHostViewMac::SetShowingContextMenu(bool showing) {
  // Create a fake mouse event to inform the render widget that the mouse
  // left or entered.
  NSWindow* window = [cocoa_view_ window];
  // TODO(asvitkine): If the location outside of the event stream doesn't
  // correspond to the current event (due to delayed event processing), then
  // this may result in a cursor flicker if there are later mouse move events
  // in the pipeline. Find a way to use the mouse location from the event that
  // dismissed the context menu.
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval event_time = [[NSApp currentEvent] timestamp];
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:location
                                 modifierFlags:0
                                     timestamp:event_time
                                  windowNumber:window_number()
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0];
  WebMouseEvent web_event = WebMouseEventBuilder::Build(event, cocoa_view_);
  web_event.SetModifiers(web_event.GetModifiers() |
                         WebInputEvent::kRelativeMotionEvent);
  ForwardMouseEvent(web_event);
}

bool RenderWidgetHostViewMac::IsPopup() const {
  return popup_type_ != blink::kWebPopupTypeNone;
}

void RenderWidgetHostViewMac::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  browser_compositor_->GetDelegatedFrameHost()->CopyFromCompositingSurface(
      src_subrect, dst_size, std::move(callback));
}

void RenderWidgetHostViewMac::ForwardMouseEvent(const WebMouseEvent& event) {
  if (host())
    host()->ForwardMouseEvent(event);

  if (event.GetType() == WebInputEvent::kMouseLeave) {
    [cocoa_view_ setToolTipAtMousePoint:nil];
    tooltip_text_.clear();
  }
}

void RenderWidgetHostViewMac::SetNeedsBeginFrames(bool needs_begin_frames) {
  needs_begin_frames_ = needs_begin_frames;
  UpdateNeedsBeginFramesInternal();
}

void RenderWidgetHostViewMac::UpdateNeedsBeginFramesInternal() {
  browser_compositor_->SetNeedsBeginFrames(needs_begin_frames_);
}

void RenderWidgetHostViewMac::OnResizeDueToAutoResizeComplete(
    const gfx::Size& new_size,
    uint64_t sequence_number) {
  browser_compositor_->UpdateForAutoResize(new_size);
  host()->DidAllocateLocalSurfaceIdForAutoResize(sequence_number);
}

void RenderWidgetHostViewMac::SetWantsAnimateOnlyBeginFrames() {
  browser_compositor_->SetWantsAnimateOnlyBeginFrames();
}

void RenderWidgetHostViewMac::KillSelf() {
  if (!weak_factory_.HasWeakPtrs()) {
    [cocoa_view_ setHidden:YES];
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RenderWidgetHostViewMac::ShutdownHost,
                              weak_factory_.GetWeakPtr()));
  }
}

bool RenderWidgetHostViewMac::GetLineBreakIndex(
    const std::vector<gfx::Rect>& bounds,
    const gfx::Range& range,
    size_t* line_break_point) {
  DCHECK(line_break_point);
  if (range.start() >= bounds.size() || range.is_reversed() || range.is_empty())
    return false;

  // We can't check line breaking completely from only rectangle array. Thus we
  // assume the line breaking as the next character's y offset is larger than
  // a threshold. Currently the threshold is determined as minimum y offset plus
  // 75% of maximum height.
  // TODO(nona): Check the threshold is reliable or not.
  // TODO(nona): Bidi support.
  const size_t loop_end_idx =
      std::min(bounds.size(), static_cast<size_t>(range.end()));
  int max_height = 0;
  int min_y_offset = std::numeric_limits<int32_t>::max();
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    max_height = std::max(max_height, bounds[idx].height());
    min_y_offset = std::min(min_y_offset, bounds[idx].y());
  }
  int line_break_threshold = min_y_offset + (max_height * 3 / 4);
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    if (bounds[idx].y() > line_break_threshold) {
      *line_break_point = idx;
      return true;
    }
  }
  return false;
}

gfx::Rect RenderWidgetHostViewMac::GetFirstRectForCompositionRange(
    const gfx::Range& range,
    gfx::Range* actual_range) {
  const TextInputManager::CompositionRangeInfo* composition_info =
      GetCompositionRangeInfo();
  if (!composition_info)
    return gfx::Rect();

  DCHECK(actual_range);
  DCHECK(!composition_info->character_bounds.empty());
  DCHECK(range.start() <= composition_info->character_bounds.size());
  DCHECK(range.end() <= composition_info->character_bounds.size());

  if (range.is_empty()) {
    *actual_range = range;
    if (range.start() == composition_info->character_bounds.size()) {
      return gfx::Rect(
          composition_info->character_bounds[range.start() - 1].right(),
          composition_info->character_bounds[range.start() - 1].y(), 0,
          composition_info->character_bounds[range.start() - 1].height());
    } else {
      return gfx::Rect(
          composition_info->character_bounds[range.start()].x(),
          composition_info->character_bounds[range.start()].y(), 0,
          composition_info->character_bounds[range.start()].height());
    }
  }

  size_t end_idx;
  if (!GetLineBreakIndex(composition_info->character_bounds, range, &end_idx)) {
    end_idx = range.end();
  }
  *actual_range = gfx::Range(range.start(), end_idx);
  gfx::Rect rect = composition_info->character_bounds[range.start()];
  for (size_t i = range.start() + 1; i < end_idx; ++i) {
    rect.Union(composition_info->character_bounds[i]);
  }
  return rect;
}

gfx::Range RenderWidgetHostViewMac::ConvertCharacterRangeToCompositionRange(
    const gfx::Range& request_range) {
  const TextInputManager::CompositionRangeInfo* composition_info =
      GetCompositionRangeInfo();
  if (!composition_info)
    return gfx::Range::InvalidRange();

  if (composition_info->range.is_empty())
    return gfx::Range::InvalidRange();

  if (composition_info->range.is_reversed())
    return gfx::Range::InvalidRange();

  if (request_range.start() < composition_info->range.start() ||
      request_range.start() > composition_info->range.end() ||
      request_range.end() > composition_info->range.end()) {
    return gfx::Range::InvalidRange();
  }

  return gfx::Range(request_range.start() - composition_info->range.start(),
                    request_range.end() - composition_info->range.start());
}

WebContents* RenderWidgetHostViewMac::GetWebContents() {
  return WebContents::FromRenderViewHost(RenderViewHost::From(host()));
}

bool RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(
    NSRange range,
    NSRect* rect,
    NSRange* actual_range) {
  if (!GetTextInputManager())
    return false;

  DCHECK(rect);
  // This exists to make IMEs more responsive, see http://crbug.com/115920
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::GetFirstRectForCharacterRange");

  const TextInputManager::TextSelection* selection = GetTextSelection();
  if (!selection)
    return false;

  const gfx::Range requested_range(range);
  // If requested range is same as caret location, we can just return it.
  if (selection->range().is_empty() && requested_range == selection->range()) {
    DCHECK(GetFocusedWidget());
    if (actual_range)
      *actual_range = range;
    *rect =
        NSRectFromCGRect(GetTextInputManager()
                             ->GetSelectionRegion(GetFocusedWidget()->GetView())
                             ->caret_rect.ToCGRect());
    return true;
  }

  const TextInputManager::CompositionRangeInfo* composition_info =
      GetCompositionRangeInfo();
  if (!composition_info || composition_info->range.is_empty()) {
    if (!selection->range().Contains(requested_range))
      return false;
    DCHECK(GetFocusedWidget());
    if (actual_range)
      *actual_range = selection->range().ToNSRange();
    *rect =
        NSRectFromCGRect(GetTextInputManager()
                             ->GetSelectionRegion(GetFocusedWidget()->GetView())
                             ->first_selection_rect.ToCGRect());
    return true;
  }

  const gfx::Range request_range_in_composition =
      ConvertCharacterRangeToCompositionRange(requested_range);
  if (request_range_in_composition == gfx::Range::InvalidRange())
    return false;

  // If firstRectForCharacterRange in WebFrame is failed in renderer,
  // ImeCompositionRangeChanged will be sent with empty vector.
  if (!composition_info || composition_info->character_bounds.empty())
    return false;
  DCHECK_EQ(composition_info->character_bounds.size(),
            composition_info->range.length());

  gfx::Range ui_actual_range;
  *rect = NSRectFromCGRect(GetFirstRectForCompositionRange(
                               request_range_in_composition,
                               &ui_actual_range).ToCGRect());
  if (actual_range) {
    *actual_range =
        gfx::Range(composition_info->range.start() + ui_actual_range.start(),
                   composition_info->range.start() + ui_actual_range.end())
            .ToNSRange();
  }
  return true;
}

bool RenderWidgetHostViewMac::ShouldContinueToPauseForFrame() {
  return browser_compositor_->ShouldContinueToPauseForFrame();
}

void RenderWidgetHostViewMac::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  [cocoa_view_ cancelComposition];

  // If the Mac Zoom feature is enabled, update it with the bounds of the
  // current focused node so that it can ensure that it's scrolled into view.
  // Don't do anything if it's an editable node, as this will be handled by
  // OnSelectionBoundsChanged instead.
  if (UAZoomEnabled() && !is_editable_node) {
    NSRect bounds = NSRectFromCGRect(node_bounds_in_screen.ToCGRect());
    UAZoomChangeFocus(&bounds, NULL, kUAZoomFocusTypeOther);
  }
}

void RenderWidgetHostViewMac::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  browser_compositor_->DidCreateNewRendererCompositorFrameSink(
      renderer_compositor_frame_sink);
}

void RenderWidgetHostViewMac::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    viz::mojom::HitTestRegionListPtr hit_test_region_list) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewMac::OnSwapCompositorFrame");

  last_frame_root_background_color_ = frame.metadata.root_background_color;
  last_scroll_offset_ = frame.metadata.root_scroll_offset;

  page_at_minimum_scale_ =
      frame.metadata.page_scale_factor == frame.metadata.min_page_scale_factor;

  browser_compositor_->GetDelegatedFrameHost()->SubmitCompositorFrame(
      local_surface_id, std::move(frame), std::move(hit_test_region_list));

  UpdateDisplayVSyncParameters();
}

void RenderWidgetHostViewMac::OnDidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  browser_compositor_->OnDidNotProduceFrame(ack);
}

void RenderWidgetHostViewMac::ClearCompositorFrame() {
  browser_compositor_->ClearCompositorFrame();
}

gfx::Vector2d RenderWidgetHostViewMac::GetOffsetFromRootSurface() {
  return gfx::Vector2d();
}

gfx::Rect RenderWidgetHostViewMac::GetBoundsInRootWindow() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return gfx::Rect();

  NSRect bounds = [enclosing_window frame];
  return FlipNSRectToRectScreen(bounds);
}

bool RenderWidgetHostViewMac::LockMouse() {
  if (mouse_locked_)
    return true;

  mouse_locked_ = true;

  // Lock position of mouse cursor and hide it.
  CGAssociateMouseAndMouseCursorPosition(NO);
  [NSCursor hide];

  // Clear the tooltip window.
  SetTooltipText(base::string16());

  return true;
}

void RenderWidgetHostViewMac::UnlockMouse() {
  if (!mouse_locked_)
    return;
  mouse_locked_ = false;

  // Unlock position of mouse cursor and unhide it.
  CGAssociateMouseAndMouseCursorPosition(YES);
  [NSCursor unhide];

  if (host())
    host()->LostMouseLock();
}

void RenderWidgetHostViewMac::GestureEventAck(const WebGestureEvent& event,
                                              InputEventAckState ack_result) {
  bool consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  switch (event.GetType()) {
    case WebInputEvent::kGestureScrollBegin:
    case WebInputEvent::kGestureScrollUpdate:
    case WebInputEvent::kGestureScrollEnd:
      [cocoa_view_ processedGestureScrollEvent:event consumed:consumed];
      return;
    default:
      break;
  }
}

void RenderWidgetHostViewMac::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  [cocoa_view_ processedOverscroll:params];
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewMac::CreateSyntheticGestureTarget() {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetMac(host, cocoa_view_));
}

viz::LocalSurfaceId RenderWidgetHostViewMac::GetLocalSurfaceId() const {
  return browser_compositor_->GetRendererLocalSurfaceId();
}

viz::FrameSinkId RenderWidgetHostViewMac::GetFrameSinkId() {
  return browser_compositor_->GetDelegatedFrameHost()->frame_sink_id();
}

bool RenderWidgetHostViewMac::ShouldRouteEvent(
    const WebInputEvent& event) const {
  // See also RenderWidgetHostViewAura::ShouldRouteEvent.
  // TODO(wjmaclean): Update this function if RenderWidgetHostViewMac implements
  // OnTouchEvent(), to match what we are doing in RenderWidgetHostViewAura.
  DCHECK(WebInputEvent::IsMouseEventType(event.GetType()) ||
         event.GetType() == WebInputEvent::kMouseWheel ||
         WebInputEvent::IsPinchGestureEventType(event.GetType()));
  return host()->delegate() && host()->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewMac::SendGesturePinchEvent(WebGestureEvent* event) {
  DCHECK(WebInputEvent::IsPinchGestureEventType(event->GetType()));
  if (ShouldRouteEvent(*event)) {
    DCHECK(event->SourceDevice() ==
           blink::WebGestureDevice::kWebGestureDeviceTouchpad);
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, event, ui::LatencyInfo(ui::SourceEventType::WHEEL));
    return;
  }
  host()->ForwardGestureEvent(*event);
}

bool RenderWidgetHostViewMac::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  // Transformations use physical pixels rather than DIP, so conversion
  // is necessary.
  float scale_factor = ui::GetScaleFactorForNativeView(cocoa_view_);
  gfx::PointF point_in_pixels = gfx::ConvertPointToPixel(scale_factor, point);
  if (!browser_compositor_->GetDelegatedFrameHost()
           ->TransformPointToLocalCoordSpace(point_in_pixels, original_surface,
                                             transformed_point))
    return false;
  *transformed_point = gfx::ConvertPointToDIP(scale_factor, *transformed_point);
  return true;
}

bool RenderWidgetHostViewMac::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return browser_compositor_->GetDelegatedFrameHost()
      ->TransformPointToCoordSpaceForView(point, target_view,
                                          transformed_point);
}

viz::FrameSinkId RenderWidgetHostViewMac::GetRootFrameSinkId() {
  return browser_compositor_->GetRootFrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewMac::GetCurrentSurfaceId() const {
  return browser_compositor_->GetDelegatedFrameHost()->GetCurrentSurfaceId();
}

bool RenderWidgetHostViewMac::Send(IPC::Message* message) {
  if (host())
    return host()->Send(message);
  delete message;
  return false;
}

void RenderWidgetHostViewMac::ShutdownHost() {
  weak_factory_.InvalidateWeakPtrs();
  host()->ShutdownAndDestroyWidget(true);
  // Do not touch any members at this point, |this| has been deleted.
}

void RenderWidgetHostViewMac::SetActive(bool active) {
  if (host()) {
    host()->SetActive(active);
    if (active) {
      if (HasFocus())
        host()->Focus();
    } else {
      host()->Blur();
    }
  }
  if (HasFocus())
    SetTextInputActive(active);
  if (!active)
    UnlockMouse();
}

void RenderWidgetHostViewMac::ShowDefinitionForSelection() {
  RenderWidgetHostViewMacDictionaryHelper helper(this);
  helper.ShowDefinitionForSelection();
}

void RenderWidgetHostViewMac::SetBackgroundColor(SkColor color) {
  // This is called by the embedding code prior to the first frame appearing,
  // to set a reasonable color to show before the web content generates its
  // first frame. This will be overridden by the web contents.
  SetBackgroundLayerColor(color);
  browser_compositor_->SetBackgroundColor(color);

  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  bool opaque = SkColorGetA(color) == SK_AlphaOPAQUE;
  if (background_is_opaque_ != opaque) {
    background_is_opaque_ = opaque;
    if (host())
      host()->SetBackgroundOpaque(opaque);
  }
}

SkColor RenderWidgetHostViewMac::background_color() const {
  // This is used to specify a color to temporarily show while waiting for web
  // content. This should never return transparent, since that will cause bugs
  // where views are initialized as having a transparent background
  // inappropriately.
  // https://crbug.com/735407
  if (background_layer_color_ == SK_ColorTRANSPARENT)
    return SK_ColorWHITE;
  return background_layer_color_;
}

void RenderWidgetHostViewMac::SetBackgroundLayerColor(SkColor color) {
  if (color == background_layer_color_)
    return;
  background_layer_color_ = color;

  ScopedCAActionDisabler disabler;
  base::ScopedCFTypeRef<CGColorRef> cg_color(
      skia::CGColorCreateFromSkColor(color));
  [background_layer_ setBackgroundColor:cg_color];
}

BrowserAccessibilityManager*
    RenderWidgetHostViewMac::CreateBrowserAccessibilityManager(
        BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  return new BrowserAccessibilityManagerMac(
      BrowserAccessibilityManagerMac::GetEmptyDocument(), delegate);
}

gfx::Point RenderWidgetHostViewMac::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) {
  NSPoint origin = NSMakePoint(bounds.x(), bounds.y());
  NSSize size = NSMakeSize(bounds.width(), bounds.height());
  origin.y = NSHeight([cocoa_view_ bounds]) - origin.y;
  NSPoint originInWindow = [cocoa_view_ convertPoint:origin toView:nil];
  NSPoint originInScreen =
      ui::ConvertPointFromWindowToScreen([cocoa_view_ window], originInWindow);
  originInScreen.y = originInScreen.y - size.height;
  return gfx::Point(originInScreen.x, originInScreen.y);
}

gfx::AcceleratedWidget
RenderWidgetHostViewMac::AccessibilityGetAcceleratedWidget() {
  if (browser_compositor_)
    return browser_compositor_->GetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

void RenderWidgetHostViewMac::SetTextInputActive(bool active) {
  const bool should_enable_password_input =
      active && GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD;
  if (should_enable_password_input)
    password_input_enabler_.reset(new ui::ScopedPasswordInputEnabler());
  else
    password_input_enabler_.reset();
}

void RenderWidgetHostViewMac::PauseForPendingResizeOrRepaintsAndDraw() {
  if (!host() || !browser_compositor_ || host()->is_hidden()) {
    return;
  }

  // Pausing for one view prevents others from receiving frames.
  // This may lead to large delays, causing overlaps. See crbug.com/352020.
  if (!allow_pause_for_resize_or_repaint_)
    return;

  // Wait for a frame of the right size to come in.
  browser_compositor_->BeginPauseForFrame(host()->auto_resize_enabled());
  host()->PauseForPendingResizeOrRepaints();
  browser_compositor_->EndPauseForFrame();
}

// static
viz::FrameSinkId
RenderWidgetHostViewMac::AllocateFrameSinkIdForGuestViewHack() {
  return ImageTransportFactory::GetInstance()
      ->GetContextFactoryPrivate()
      ->AllocateFrameSinkId();
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver, public:

void RenderWidgetHostViewMac::OnDisplayAdded(const display::Display& display) {}

void RenderWidgetHostViewMac::OnDisplayRemoved(
    const display::Display& display) {}

void RenderWidgetHostViewMac::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  display::Screen* screen = display::Screen::GetScreen();
  if (display.id() != screen->GetDisplayNearestView(cocoa_view_).id())
    return;
  UpdateNSViewAndDisplayProperties();
}

Class GetRenderWidgetHostViewCocoaClassForTesting() {
  return [RenderWidgetHostViewCocoa class];
}

}  // namespace content
