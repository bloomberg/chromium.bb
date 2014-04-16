// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#import <objc/runtime.h>
#include <QuartzCore/QuartzCore.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"
#import "content/browser/renderer_host/text_input_client_mac.h"
#include "content/common/accessibility_messages.h"
#include "content/common/edit_command.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/common/webplugin_geometry.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/mac/WebInputEventFactory.h"
#import "third_party/mozilla/ComplexTextInputPanel.h"
#include "ui/base/cocoa/animation_utils.h"
#import "ui/base/cocoa/fullscreen_window_manager.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/base/layout.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/io_surface_support_mac.h"

using content::BrowserAccessibility;
using content::BrowserAccessibilityManager;
using content::EditCommand;
using content::FrameTreeNode;
using content::NativeWebKeyboardEvent;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::RenderViewHostImpl;
using content::RenderWidgetHostImpl;
using content::RenderWidgetHostViewMac;
using content::RenderWidgetHostViewMacEditCommandHelper;
using content::TextInputClientMac;
using content::WebContents;
using blink::WebInputEvent;
using blink::WebInputEventFactory;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebGestureEvent;

// These are not documented, so use only after checking -respondsToSelector:.
@interface NSApplication (UndocumentedSpeechMethods)
- (void)speakString:(NSString*)string;
- (void)stopSpeaking:(id)sender;
- (BOOL)isSpeaking;
@end

// Declare things that are part of the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
@interface NSView (NSOpenGLSurfaceResolutionLionAPI)
- (void)setWantsBestResolutionOpenGLSurface:(BOOL)flag;
@end

static NSString* const NSWindowDidChangeBackingPropertiesNotification =
    @"NSWindowDidChangeBackingPropertiesNotification";

#endif  // 10.7

// This method will return YES for OS X versions 10.7.3 and later, and NO
// otherwise.
// Used to prevent a crash when building with the 10.7 SDK and accessing the
// notification below. See: http://crbug.com/260595.
static BOOL SupportsBackingPropertiesChangedNotification() {
  // windowDidChangeBackingProperties: method has been added to the
  // NSWindowDelegate protocol in 10.7.3, at the same time as the
  // NSWindowDidChangeBackingPropertiesNotification notification was added.
  // If the protocol contains this method description, the notification should
  // be supported as well.
  Protocol* windowDelegateProtocol = NSProtocolFromString(@"NSWindowDelegate");
  struct objc_method_description methodDescription =
      protocol_getMethodDescription(
          windowDelegateProtocol,
          @selector(windowDidChangeBackingProperties:),
          NO,
          YES);

  // If the protocol does not contain the method, the returned method
  // description is {NULL, NULL}
  return methodDescription.name != NULL || methodDescription.types != NULL;
}

static float ScaleFactorForView(NSView* view) {
  return ui::GetImageScale(ui::GetScaleFactorForNativeView(view));
}

// Private methods:
@interface RenderWidgetHostViewCocoa ()
@property(nonatomic, assign) NSRange selectedRange;
@property(nonatomic, assign) NSRange markedRange;

+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event;
- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r;
- (void)gotWheelEventConsumed:(BOOL)consumed;
- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;
- (void)windowChangedGlobalFrame:(NSNotification*)notification;
- (void)drawWithDirtyRect:(CGRect)dirtyRect
                inContext:(CGContextRef)context;
- (void)checkForPluginImeCancellation;
- (void)updateScreenProperties;
- (void)setResponderDelegate:
        (NSObject<RenderWidgetHostViewMacDelegate>*)delegate;
@end

// A window subclass that allows the fullscreen window to become main and gain
// keyboard focus. This is only used for pepper flash. Normal fullscreen is
// handled by the browser.
@interface PepperFlashFullscreenWindow : UnderlayOpenGLHostingWindow
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
    DCHECK_EQ(content::CORE_ANIMATION_DISABLED,
              content::GetCoreAnimationStatus());
    [self setOpaque:NO];
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

// TODO(suzhe): Upstream this function.
blink::WebColor WebColorFromNSColor(NSColor *color) {
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];

  return
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * a)), 255)) << 24 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * r)), 255)) << 16 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * g)), 255)) << 8  |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * b)), 255));
}

// Extract underline information from an attributed string. Mostly copied from
// third_party/WebKit/Source/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(
    NSAttributedString* string,
    std::vector<blink::WebCompositionUnderline>* underlines) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
      blink::WebColor color = SK_ColorBLACK;
      if (NSColor *colorAttr =
          [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = WebColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      underlines->push_back(blink::WebCompositionUnderline(
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

// Calls to [NSScreen screens], required by FlipYFromRectToScreen and
// FlipNSRectToRectScreen, can take several milliseconds. Only re-compute this
// value when screen info changes.
// TODO(ccameron): An observer on every RWHVCocoa will set this to false
// on NSApplicationDidChangeScreenParametersNotification. Only one observer
// is necessary.
bool g_screen_info_up_to_date = false;

float FlipYFromRectToScreen(float y, float rect_height) {
  TRACE_EVENT0("browser", "FlipYFromRectToScreen");
  static CGFloat screen_zero_height = 0;
  if (!g_screen_info_up_to_date) {
    if ([[NSScreen screens] count] > 0) {
      screen_zero_height =
          [[[NSScreen screens] objectAtIndex:0] frame].size.height;
      g_screen_info_up_to_date = true;
    } else {
      return y;
    }
  }
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

blink::WebScreenInfo GetWebScreenInfo(NSView* view) {
  gfx::Display display =
      gfx::Screen::GetNativeScreen()->GetDisplayNearestWindow(view);

  NSScreen* screen = [NSScreen deepestScreen];

  blink::WebScreenInfo results;

  results.deviceScaleFactor = static_cast<int>(display.device_scale_factor());
  results.depth = NSBitsPerPixelFromDepth([screen depth]);
  results.depthPerComponent = NSBitsPerSampleFromDepth([screen depth]);
  results.isMonochrome =
      [[screen colorSpace] colorSpaceModel] == NSGrayColorSpaceModel;
  results.rect = display.bounds();
  results.availableRect = display.work_area();
  return results;
}

void RemoveLayerFromSuperlayer(
    base::scoped_nsobject<CompositingIOSurfaceLayer> layer) {
  // Disable the fade-out animation as the layer is removed.
  ScopedCAActionDisabler disabler;
  [layer removeFromSuperlayer];
}

}  // namespace

namespace content {

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(
    blink::WebScreenInfo* results) {
  *results = GetWebScreenInfo(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget)
    : render_widget_host_(RenderWidgetHostImpl::From(widget)),
      last_frame_was_accelerated_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      can_compose_inline_(true),
      compositing_iosurface_layer_async_timer_(
            FROM_HERE, base::TimeDelta::FromMilliseconds(250),
            this, &RenderWidgetHostViewMac::TimerSinceGotAcceleratedFrameFired),
      allow_overlapping_views_(false),
      use_core_animation_(false),
      pending_latency_info_delay_(0),
      pending_latency_info_delay_weak_ptr_factory_(this),
      backing_store_scale_factor_(1),
      is_loading_(false),
      weak_factory_(this),
      fullscreen_parent_host_view_(NULL),
      underlay_view_has_drawn_(false),
      overlay_view_weak_factory_(this),
      software_frame_weak_ptr_factory_(this) {
  software_frame_manager_.reset(new SoftwareFrameManager(
      software_frame_weak_ptr_factory_.GetWeakPtr()));
  // |cocoa_view_| owns us and we will be deleted when |cocoa_view_|
  // goes away.  Since we autorelease it, our caller must put
  // |GetNativeView()| into the view hierarchy right after calling us.
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];

  if (GetCoreAnimationStatus() == CORE_ANIMATION_ENABLED) {
    use_core_animation_ = true;
    background_layer_.reset([[CALayer alloc] init]);
    [background_layer_
        setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [cocoa_view_ setLayer:background_layer_];
    [cocoa_view_ setWantsLayer:YES];
  }

  render_widget_host_->SetView(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
  // This is being called from |cocoa_view_|'s destructor, so invalidate the
  // pointer.
  cocoa_view_ = nil;

  UnlockMouse();

  // Make sure that the layer doesn't reach into the now-invalid object.
  DestroyCompositedIOSurfaceAndLayer(kDestroyContext);
  DestroySoftwareLayer();

  // We are owned by RenderWidgetHostViewCocoa, so if we go away before the
  // RenderWidgetHost does we need to tell it not to hold a stale pointer to
  // us.
  if (render_widget_host_)
    render_widget_host_->SetView(NULL);
}

void RenderWidgetHostViewMac::SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate) {
  [cocoa_view_ setResponderDelegate:delegate];
}

void RenderWidgetHostViewMac::SetAllowOverlappingViews(bool overlapping) {
  if (allow_overlapping_views_ == overlapping)
    return;
  allow_overlapping_views_ = overlapping;
  [cocoa_view_ setNeedsDisplay:YES];
  [[cocoa_view_ window] disableScreenUpdatesUntilFlush];
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

bool RenderWidgetHostViewMac::EnsureCompositedIOSurface() {
  // If the context or the IOSurface's context has had an error, re-build
  // everything from scratch.
  if (compositing_iosurface_context_ &&
      compositing_iosurface_context_->HasBeenPoisoned()) {
    LOG(ERROR) << "Failing EnsureCompositedIOSurface because "
               << "context was poisoned";
    return false;
  }
  if (compositing_iosurface_ &&
      compositing_iosurface_->HasBeenPoisoned()) {
    LOG(ERROR) << "Failing EnsureCompositedIOSurface because "
               << "surface was poisoned";
    return false;
  }

  int current_window_number = use_core_animation_ ?
      CompositingIOSurfaceContext::kOffscreenContextWindowNumber :
      window_number();
  bool new_surface_needed = !compositing_iosurface_;
  bool new_context_needed =
    !compositing_iosurface_context_ ||
        (compositing_iosurface_context_ &&
            compositing_iosurface_context_->window_number() !=
                current_window_number);

  if (!new_surface_needed && !new_context_needed)
    return true;

  // Create the GL context and shaders.
  if (new_context_needed) {
    scoped_refptr<CompositingIOSurfaceContext> new_context =
        CompositingIOSurfaceContext::Get(current_window_number);
    // Un-bind the GL context from this view before binding the new GL
    // context. Having two GL contexts bound to a view will result in
    // crashes and corruption.
    // http://crbug.com/230883
    ClearBoundContextDrawable();
    if (!new_context) {
      LOG(ERROR) << "Failed to create CompositingIOSurfaceContext";
      return false;
    }
    compositing_iosurface_context_ = new_context;
  }

  // Create the IOSurface texture.
  if (new_surface_needed) {
    compositing_iosurface_.reset(CompositingIOSurfaceMac::Create());
    if (!compositing_iosurface_) {
      LOG(ERROR) << "Failed to create CompositingIOSurface";
      return false;
    }
  }

  return true;
}

void RenderWidgetHostViewMac::EnsureSoftwareLayer() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewMac::EnsureSoftwareLayer");
  if (software_layer_ || !use_core_animation_)
    return;

  software_layer_.reset([[SoftwareLayer alloc]
      initWithRenderWidgetHostViewMac:this]);
  DCHECK(software_layer_);

  // Disable the fade-in animation as the layer is added.
  ScopedCAActionDisabler disabler;
  [background_layer_ addSublayer:software_layer_];
}

void RenderWidgetHostViewMac::DestroySoftwareLayer() {
  if (!software_layer_)
    return;

  // Disable the fade-out animation as the layer is removed.
  ScopedCAActionDisabler disabler;
  [software_layer_ removeFromSuperlayer];
  [software_layer_ disableRendering];
  software_layer_.reset();
}

void RenderWidgetHostViewMac::EnsureCompositedIOSurfaceLayer() {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::EnsureCompositedIOSurfaceLayer");
  DCHECK(compositing_iosurface_context_);
  if (compositing_iosurface_layer_ || !use_core_animation_)
    return;

  compositing_iosurface_layer_.reset([[CompositingIOSurfaceLayer alloc]
      initWithRenderWidgetHostViewMac:this]);
  DCHECK(compositing_iosurface_layer_);

  // Disable the fade-in animation as the layer is added.
  ScopedCAActionDisabler disabler;
  [background_layer_ addSublayer:compositing_iosurface_layer_];
}

void RenderWidgetHostViewMac::DestroyCompositedIOSurfaceLayer(
    DestroyCompositedIOSurfaceLayerBehavior destroy_layer_behavior) {
  if (!compositing_iosurface_layer_)
    return;

  if (destroy_layer_behavior == kRemoveLayerFromHierarchy) {
    // Disable the fade-out animation as the layer is removed.
    ScopedCAActionDisabler disabler;
    [compositing_iosurface_layer_ removeFromSuperlayer];
  }
  [compositing_iosurface_layer_ disableCompositing];
  compositing_iosurface_layer_.reset();
}

void RenderWidgetHostViewMac::DestroyCompositedIOSurfaceAndLayer(
    DestroyContextBehavior destroy_context_behavior) {
  // Any pending frames will not be displayed, so ack them now.
  SendPendingSwapAck();

  DestroyCompositedIOSurfaceLayer(kRemoveLayerFromHierarchy);
  compositing_iosurface_.reset();

  switch (destroy_context_behavior) {
    case kLeaveContextBoundToView:
      break;
    case kDestroyContext:
      ClearBoundContextDrawable();
      compositing_iosurface_context_ = NULL;
      break;
    default:
      NOTREACHED();
      break;
  }
}

void RenderWidgetHostViewMac::ClearBoundContextDrawable() {
  if (use_core_animation_)
    return;

  if (compositing_iosurface_context_ &&
      cocoa_view_ &&
      [[compositing_iosurface_context_->nsgl_context() view]
          isEqual:cocoa_view_]) {
    // Disable screen updates because removing the GL context from below can
    // cause flashes.
    [[cocoa_view_ window] disableScreenUpdatesUntilFlush];
    [compositing_iosurface_context_->nsgl_context() clearDrawable];
  }
}

bool RenderWidgetHostViewMac::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewMac, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginFocusChanged, OnPluginFocusChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartPluginIme, OnStartPluginIme)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewMac::InitAsChild(
    gfx::NativeView parent_view) {
}

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  bool activatable = popup_type_ == blink::WebPopupTypeNone;
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
  // explicitly calls Shutdown on the render_widget_host_, which calls
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

float RenderWidgetHostViewMac::ViewScaleFactor() const {
  return ScaleFactorForView(cocoa_view_);
}

void RenderWidgetHostViewMac::UpdateDisplayLink() {
  static bool is_vsync_disabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync);
  if (is_vsync_disabled)
    return;

  NSScreen* screen = [[cocoa_view_ window] screen];
  NSDictionary* screen_description = [screen deviceDescription];
  NSNumber* screen_number = [screen_description objectForKey:@"NSScreenNumber"];
  CGDirectDisplayID display_id = [screen_number unsignedIntValue];

  display_link_ = DisplayLinkMac::GetForDisplay(display_id);
  if (!display_link_) {
    // Note that on some headless systems, the display link will fail to be
    // created, so this should not be a fatal error.
    LOG(ERROR) << "Failed to create display link.";
  }
}

void RenderWidgetHostViewMac::SendVSyncParametersToRenderer() {
  if (!render_widget_host_ || !display_link_)
    return;

  base::TimeTicks timebase;
  base::TimeDelta interval;
  if (!display_link_->GetVSyncParameters(&timebase, &interval))
    return;

  render_widget_host_->UpdateVSyncParameters(timebase, interval);
}

void RenderWidgetHostViewMac::UpdateBackingStoreScaleFactor() {
  if (!render_widget_host_)
    return;

  float new_scale_factor = ScaleFactorForView(cocoa_view_);
  if (new_scale_factor == backing_store_scale_factor_)
    return;
  backing_store_scale_factor_ = new_scale_factor;

  render_widget_host_->NotifyScreenInfoChanged();
}

RenderWidgetHost* RenderWidgetHostViewMac::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewMac::WasShown() {
  if (!render_widget_host_->is_hidden())
    return;

  if (web_contents_switch_paint_time_.is_null())
    web_contents_switch_paint_time_ = base::TimeTicks::Now();
  render_widget_host_->WasShown();
  software_frame_manager_->SetVisibility(true);

  // Call setNeedsDisplay before pausing for new frames to come in -- if any
  // do, and are drawn, then the needsDisplay bit will be cleared.
  [software_layer_ setNeedsDisplay];
  [compositing_iosurface_layer_ setNeedsDisplay];
  PauseForPendingResizeOrRepaintsAndDraw();

  // We're messing with the window, so do this to ensure no flashes.
  if (!use_core_animation_)
    [[cocoa_view_ window] disableScreenUpdatesUntilFlush];
}

void RenderWidgetHostViewMac::WasHidden() {
  if (render_widget_host_->is_hidden())
    return;

  // Any pending frames will not be displayed until this is shown again. Ack
  // them now.
  SendPendingSwapAck();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  render_widget_host_->WasHidden();
  software_frame_manager_->SetVisibility(false);

  // There can be a transparent flash as this view is removed and the next is
  // added, because of OSX windowing races between displaying the contents of
  // the NSView and its corresponding OpenGL context.
  // disableScreenUpdatesUntilFlush prevents the transparent flash by avoiding
  // screen updates until the next tab draws.
  if (!use_core_animation_)
    [[cocoa_view_ window] disableScreenUpdatesUntilFlush];

  web_contents_switch_paint_time_ = base::TimeTicks();
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  gfx::Rect rect = GetViewBounds();
  rect.set_size(size);
  SetBounds(rect);
}

void RenderWidgetHostViewMac::SetBounds(const gfx::Rect& rect) {
  // |rect.size()| is view coordinates, |rect.origin| is screen coordinates,
  // TODO(thakis): fix, http://crbug.com/73362
  if (render_widget_host_->is_hidden())
    return;

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

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() const {
  return cocoa_view_;
}

gfx::NativeViewId RenderWidgetHostViewMac::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(GetNativeView());
}

gfx::NativeViewAccessible RenderWidgetHostViewMac::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return static_cast<gfx::NativeViewAccessible>(NULL);
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<WebPluginGeometry>& moves) {
  // Must be overridden, but unused on this platform. Core Animation
  // plugins are drawn by the GPU process (through the compositor),
  // and Core Graphics plugins are drawn by the renderer process.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

void RenderWidgetHostViewMac::Blur() {
  UnlockMouse();
  [[cocoa_view_ window] makeFirstResponder:nil];
}

bool RenderWidgetHostViewMac::HasFocus() const {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

bool RenderWidgetHostViewMac::IsSurfaceAvailableForCopy() const {
  return software_frame_manager_->HasCurrentFrame() ||
         (compositing_iosurface_ && compositing_iosurface_->HasIOSurface());
}

void RenderWidgetHostViewMac::Show() {
  [cocoa_view_ setHidden:NO];

  WasShown();
}

void RenderWidgetHostViewMac::Hide() {
  // We're messing with the window, so do this to ensure no flashes.
  if (!use_core_animation_)
    [[cocoa_view_ window] disableScreenUpdatesUntilFlush];

  [cocoa_view_ setHidden:YES];

  WasHidden();
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
  bounds.origin = [enclosing_window convertBaseToScreen:bounds.origin];
  return FlipNSRectToRectScreen(bounds);
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  WebCursor web_cursor = cursor;
  [cocoa_view_ updateCursor:web_cursor.GetNativeCursor()];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // If we ever decide to show the waiting cursor while the page is loading
  // like Chrome does on Windows, call |UpdateCursor()| here.
}

void RenderWidgetHostViewMac::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline) {
  if (text_input_type_ != type
      || can_compose_inline_ != can_compose_inline) {
    text_input_type_ = type;
    can_compose_inline_ = can_compose_inline;
    if (HasFocus()) {
      SetTextInputActive(true);

      // Let AppKit cache the new input context to make IMEs happy.
      // See http://crbug.com/73039.
      [NSApp updateWindows];

#ifndef __LP64__
      UseInputWindow(TSMGetActiveDocument(), !can_compose_inline_);
#endif
    }
  }
}

void RenderWidgetHostViewMac::ImeCancelComposition() {
  [cocoa_view_ cancelComposition];
}

void RenderWidgetHostViewMac::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // The RangeChanged message is only sent with valid values. The current
  // caret position (start == end) will be sent if there is no IME range.
  [cocoa_view_ setMarkedRange:range.ToNSRange()];
  composition_range_ = range;
  composition_bounds_ = character_bounds;
}

void RenderWidgetHostViewMac::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects,
    const std::vector<ui::LatencyInfo>& latency_info) {
  // This can be called while already inside of a display function. Process the
  // new frame in a callback to ensure a clean stack.
  // TODO(ccameron): This should never be called. Remove the remaining callers
  // and remove all places where the backing store is drawn.
  AddPendingLatencyInfo(latency_info);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RenderWidgetHostViewMac::GotSoftwareFrame,
                 weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostViewMac::RenderProcessGone(base::TerminationStatus status,
                                                int error_code) {
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
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

  // We get this call just before |render_widget_host_| deletes
  // itself.  But we are owned by |cocoa_view_|, which may be retained
  // by some other code.  Examples are WebContentsViewMac's
  // |latent_focus_view_| and TabWindowController's
  // |cachedContentView_|.
  render_widget_host_ = NULL;
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

bool RenderWidgetHostViewMac::SupportsSpeech() const {
  return [NSApp respondsToSelector:@selector(speakString:)] &&
         [NSApp respondsToSelector:@selector(stopSpeaking:)];
}

void RenderWidgetHostViewMac::SpeakSelection() {
  if ([NSApp respondsToSelector:@selector(speakString:)])
    [NSApp speakString:base::SysUTF8ToNSString(selected_text_)];
}

bool RenderWidgetHostViewMac::IsSpeaking() const {
  return [NSApp respondsToSelector:@selector(isSpeaking)] &&
         [NSApp isSpeaking];
}

void RenderWidgetHostViewMac::StopSpeaking() {
  if ([NSApp respondsToSelector:@selector(stopSpeaking:)])
    [NSApp stopSpeaking:cocoa_view_];
}

//
// RenderWidgetHostViewCocoa uses the stored selection text,
// which implements NSServicesRequests protocol.
//
void RenderWidgetHostViewMac::SelectionChanged(const base::string16& text,
                                               size_t offset,
                                               const gfx::Range& range) {
  if (range.is_empty() || text.empty()) {
    selected_text_.clear();
  } else {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();

    DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
    if (pos >= text.length()) {
      DCHECK(false) << "The text can not cover range.";
      return;
    }
    selected_text_ = base::UTF16ToUTF8(text.substr(pos, n));
  }

  [cocoa_view_ setSelectedRange:range.ToNSRange()];
  // Updates markedRange when there is no marked text so that retrieving
  // markedRange immediately after calling setMarkdText: returns the current
  // caret position.
  if (![cocoa_view_ hasMarkedText]) {
    [cocoa_view_ setMarkedRange:range.ToNSRange()];
  }

  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);
}

void RenderWidgetHostViewMac::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (params.anchor_rect == params.focus_rect)
    caret_rect_ = params.anchor_rect;
}

void RenderWidgetHostViewMac::ScrollOffsetChanged() {
}

void RenderWidgetHostViewMac::SetShowingContextMenu(bool showing) {
  RenderWidgetHostViewBase::SetShowingContextMenu(showing);

  // Create a fake mouse event to inform the render widget that the mouse
  // left or entered.
  NSWindow* window = [cocoa_view_ window];
  // TODO(asvitkine): If the location outside of the event stream doesn't
  // correspond to the current event (due to delayed event processing), then
  // this may result in a cursor flicker if there are later mouse move events
  // in the pipeline. Find a way to use the mouse location from the event that
  // dismissed the context menu.
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:location
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:window_number()
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0];
  WebMouseEvent web_event =
      WebInputEventFactory::mouseEvent(event, cocoa_view_);
  if (showing)
    web_event.type = WebInputEvent::MouseLeave;
  ForwardMouseEvent(web_event);
}

bool RenderWidgetHostViewMac::IsPopup() const {
  return popup_type_ != blink::WebPopupTypeNone;
}

void RenderWidgetHostViewMac::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config config) {
  if (config != SkBitmap::kARGB_8888_Config) {
    NOTIMPLEMENTED();
    callback.Run(false, SkBitmap());
  }
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));
  float scale = ScaleFactorForView(cocoa_view_);
  gfx::Size dst_pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(dst_size, scale));
  if (compositing_iosurface_ && compositing_iosurface_->HasIOSurface()) {
    ignore_result(scoped_callback_runner.Release());
    compositing_iosurface_->CopyTo(GetScaledOpenGLPixelRect(src_subrect),
                                   dst_pixel_size,
                                   callback);
  } else if (software_frame_manager_->HasCurrentFrame()) {
    gfx::Rect src_pixel_rect = gfx::ToEnclosingRect(gfx::ScaleRect(
        src_subrect,
        software_frame_manager_->GetCurrentFrameDeviceScaleFactor()));
    SkBitmap source_bitmap;
    source_bitmap.setConfig(
        SkBitmap::kARGB_8888_Config,
        software_frame_manager_->GetCurrentFrameSizeInPixels().width(),
        software_frame_manager_->GetCurrentFrameSizeInPixels().height(),
        0,
        kOpaque_SkAlphaType);
    source_bitmap.setPixels(software_frame_manager_->GetCurrentFramePixels());

    SkBitmap target_bitmap;
    target_bitmap.setConfig(
        SkBitmap::kARGB_8888_Config,
        dst_pixel_size.width(),
        dst_pixel_size.height(),
        0,
        kOpaque_SkAlphaType);
    if (!target_bitmap.allocPixels())
      return;

    SkCanvas target_canvas(target_bitmap);
    SkRect src_pixel_skrect = SkRect::MakeXYWH(
        src_pixel_rect.x(), src_pixel_rect.y(),
        src_pixel_rect.width(), src_pixel_rect.height());
    target_canvas.drawBitmapRectToRect(
        source_bitmap,
        &src_pixel_skrect,
        SkRect::MakeXYWH(0, 0, dst_pixel_size.width(), dst_pixel_size.height()),
        NULL,
        SkCanvas::kNone_DrawBitmapRectFlag);

    ignore_result(scoped_callback_runner.Release());
    callback.Run(true, target_bitmap);
  }
}

void RenderWidgetHostViewMac::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  base::ScopedClosureRunner scoped_callback_runner(base::Bind(callback, false));
  if (!render_widget_host_->is_accelerated_compositing_active() ||
      !compositing_iosurface_ ||
      !compositing_iosurface_->HasIOSurface())
    return;

  if (!target.get()) {
    NOTREACHED();
    return;
  }

  if (target->format() != media::VideoFrame::YV12 &&
      target->format() != media::VideoFrame::I420) {
    NOTREACHED();
    return;
  }

  if (src_subrect.IsEmpty())
    return;

  ignore_result(scoped_callback_runner.Release());
  compositing_iosurface_->CopyToVideoFrame(
      GetScaledOpenGLPixelRect(src_subrect),
      target,
      callback);
}

bool RenderWidgetHostViewMac::CanCopyToVideoFrame() const {
  return (!software_frame_manager_->HasCurrentFrame() &&
          render_widget_host_->is_accelerated_compositing_active() &&
          compositing_iosurface_ &&
          compositing_iosurface_->HasIOSurface());
}

bool RenderWidgetHostViewMac::CanSubscribeFrame() const {
  return !software_frame_manager_->HasCurrentFrame();
}

void RenderWidgetHostViewMac::BeginFrameSubscription(
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscriber_ = subscriber.Pass();
}

void RenderWidgetHostViewMac::EndFrameSubscription() {
  frame_subscriber_.reset();
}

// Sets whether or not to accept first responder status.
void RenderWidgetHostViewMac::SetTakesFocusOnlyOnMouseDown(bool flag) {
  [cocoa_view_ setTakesFocusOnlyOnMouseDown:flag];
}

void RenderWidgetHostViewMac::ForwardMouseEvent(const WebMouseEvent& event) {
  if (render_widget_host_)
    render_widget_host_->ForwardMouseEvent(event);

  if (event.type == WebInputEvent::MouseLeave) {
    [cocoa_view_ setToolTipAtMousePoint:nil];
    tooltip_text_.clear();
  }
}

void RenderWidgetHostViewMac::KillSelf() {
  if (!weak_factory_.HasWeakPtrs()) {
    [cocoa_view_ setHidden:YES];
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&RenderWidgetHostViewMac::ShutdownHost,
                   weak_factory_.GetWeakPtr()));
  }
}

bool RenderWidgetHostViewMac::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  // Check WebInputEvent type since multiple types of events can be sent into
  // WebKit for the same OS event (e.g., RawKeyDown and Char), so filtering is
  // necessary to avoid double processing.
  // Also check the native type, since NSFlagsChanged is considered a key event
  // for WebKit purposes, but isn't considered a key event by the OS.
  if (event.type == WebInputEvent::RawKeyDown &&
      [event.os_event type] == NSKeyDown)
    return [cocoa_view_ postProcessEventForPluginIme:event.os_event];
  return false;
}

void RenderWidgetHostViewMac::PluginImeCompositionCompleted(
    const base::string16& text, int plugin_id) {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_PluginImeCompositionCompleted(
        render_widget_host_->GetRoutingID(), text, plugin_id));
  }
}

void RenderWidgetHostViewMac::CompositorSwapBuffers(
    uint64 surface_handle,
    const gfx::Size& size,
    float surface_scale_factor,
    const std::vector<ui::LatencyInfo>& latency_info) {
  // Ensure that the frame be acked unless it is explicitly passed to a
  // display function.
  base::ScopedClosureRunner scoped_ack(
      base::Bind(&RenderWidgetHostViewMac::SendPendingSwapAck,
                 weak_factory_.GetWeakPtr()));

  if (render_widget_host_->is_hidden())
    return;

  // Ensure that if this function exits before the frame is set up (but not
  // necessarily drawn) then it is treated as an error.
  base::ScopedClosureRunner scoped_error(
      base::Bind(&RenderWidgetHostViewMac::GotAcceleratedCompositingError,
                 weak_factory_.GetWeakPtr()));

  AddPendingLatencyInfo(latency_info);

  // If compositing_iosurface_ exists and has been poisoned, destroy it
  // and allow EnsureCompositedIOSurface to recreate it below. Keep a
  // reference to the destroyed layer around until after the below call
  // to LayoutLayers, to avoid flickers.
  base::ScopedClosureRunner scoped_layer_remover;
  if (compositing_iosurface_context_ &&
      compositing_iosurface_context_->HasBeenPoisoned()) {
    scoped_layer_remover.Reset(
        base::Bind(RemoveLayerFromSuperlayer, compositing_iosurface_layer_));
    DestroyCompositedIOSurfaceLayer(kLeaveLayerInHierarchy);
    DestroyCompositedIOSurfaceAndLayer(kDestroyContext);
  }

  // Ensure compositing_iosurface_ and compositing_iosurface_context_ be
  // allocated.
  if (!EnsureCompositedIOSurface()) {
    LOG(ERROR) << "Failed EnsureCompositingIOSurface";
    return;
  }

  // Make the context current and update the IOSurface with the handle
  // passed in by the swap command.
  {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
        compositing_iosurface_context_->cgl_context());
    if (!compositing_iosurface_->SetIOSurfaceWithContextCurrent(
            compositing_iosurface_context_, surface_handle, size,
            surface_scale_factor)) {
      LOG(ERROR) << "Failed SetIOSurface on CompositingIOSurfaceMac";
      return;
    }
  }

  // Grab video frames now that the IOSurface has been set up. Note that this
  // will be done in an offscreen context, so it is necessary to re-set the
  // current context afterward.
  bool frame_was_captured = false;
  if (frame_subscriber_) {
    const base::TimeTicks present_time = base::TimeTicks::Now();
    scoped_refptr<media::VideoFrame> frame;
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    if (frame_subscriber_->ShouldCaptureFrame(present_time,
                                              &frame, &callback)) {
      // Flush the context that updated the IOSurface, to ensure that the
      // context that does the copy picks up the correct version.
      {
        gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
            compositing_iosurface_context_->cgl_context());
        glFlush();
      }
      compositing_iosurface_->CopyToVideoFrame(
          gfx::Rect(size), frame,
          base::Bind(callback, present_time));
      frame_was_captured = true;
    }
  }

  // At this point the surface, its context, and its layer have been set up, so
  // don't generate an error (one may be generated when drawing).
  ignore_result(scoped_error.Release());

  GotAcceleratedFrame();

  gfx::Size window_size(NSSizeToCGSize([cocoa_view_ frame].size));
  if (window_size.IsEmpty()) {
    // setNeedsDisplay will never display and we'll never ack if the window is
    // empty, so ack now and don't bother calling setNeedsDisplay below.
    return;
  }
  if (window_number() <= 0) {
    // It's normal for a backgrounded tab that is being captured to have no
    // window but not be hidden. Immediately ack the frame, and don't try to
    // draw it.
    if (frame_was_captured)
      return;

    // If this frame was not captured, there is likely some sort of bug. Ack
    // the frame and hope for the best. Because the IOSurface and layer are
    // populated, it will likely be displayed when the view is added to a
    // window's hierarchy.

    // TODO(shess) If the view does not have a window, or the window
    // does not have backing, the IOSurface will log "invalid drawable"
    // in -setView:.  It is not clear how this code is reached with such
    // a case, so record some info into breakpad (some subset of
    // browsers are likely to crash later for unrelated reasons).
    // http://crbug.com/148882
    const char* const kCrashKey = "rwhvm_window";
    NSWindow* window = [cocoa_view_ window];
    if (!window) {
      base::debug::SetCrashKeyValue(kCrashKey, "Missing window");
    } else {
      std::string value =
          base::StringPrintf("window %s delegate %s controller %s",
              object_getClassName(window),
              object_getClassName([window delegate]),
              object_getClassName([window windowController]));
      base::debug::SetCrashKeyValue(kCrashKey, value);
    }
    return;
  }

  // If we reach here, then the frame will be displayed by a future draw
  // call, so don't make the callback.
  ignore_result(scoped_ack.Release());
  if (use_core_animation_) {
    DCHECK(compositing_iosurface_layer_);
    compositing_iosurface_layer_async_timer_.Reset();
    [compositing_iosurface_layer_ gotNewFrame];
  } else {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
        compositing_iosurface_context_->cgl_context());
    DrawIOSurfaceWithoutCoreAnimation();
  }

  // Try to finish previous copy requests after draw to get better pipelining.
  if (compositing_iosurface_)
    compositing_iosurface_->CheckIfAllCopiesAreFinished(false);

  // The IOSurface's size may have changed, so re-layout the layers to take
  // this into account. This may force an immediate draw.
  LayoutLayers();
}

void RenderWidgetHostViewMac::DrawIOSurfaceWithoutCoreAnimation() {
  CHECK(!use_core_animation_);
  CHECK(compositing_iosurface_);

  // If there is a pending frame, it should be acked by the end of this
  // function. Note that the ack should happen only after all drawing is
  // complete, so that the ack happens after any blocking due to vsync.
  base::ScopedClosureRunner scoped_ack(
      base::Bind(&RenderWidgetHostViewMac::SendPendingSwapAck,
                 weak_factory_.GetWeakPtr()));

  GLint old_gl_surface_order = 0;
  GLint new_gl_surface_order = allow_overlapping_views_ ? -1 : 1;
  [compositing_iosurface_context_->nsgl_context()
      getValues:&old_gl_surface_order
      forParameter:NSOpenGLCPSurfaceOrder];
  if (old_gl_surface_order != new_gl_surface_order) {
    [compositing_iosurface_context_->nsgl_context()
        setValues:&new_gl_surface_order
        forParameter:NSOpenGLCPSurfaceOrder];
  }

  // Instead of drawing, request that underlay view redraws.
  if (underlay_view_ &&
      underlay_view_->compositing_iosurface_ &&
      underlay_view_has_drawn_) {
    [underlay_view_->cocoa_view() setNeedsDisplayInRect:NSMakeRect(0, 0, 1, 1)];
    return;
  }

  bool has_overlay = overlay_view_ && overlay_view_->compositing_iosurface_;
  if (has_overlay) {
    // Un-bind the overlay view's OpenGL context, since its content will be
    // drawn by this context. Not doing this can result in corruption.
    // http://crbug.com/330701
    overlay_view_->ClearBoundContextDrawable();
  }
  [compositing_iosurface_context_->nsgl_context() setView:cocoa_view_];

  gfx::Rect view_rect(NSRectToCGRect([cocoa_view_ frame]));
  if (!compositing_iosurface_->DrawIOSurface(
          compositing_iosurface_context_, view_rect,
          ViewScaleFactor(), !has_overlay)) {
    GotAcceleratedCompositingError();
    return;
  }

  if (has_overlay) {
    overlay_view_->underlay_view_has_drawn_ = true;
    gfx::Rect overlay_view_rect(
        NSRectToCGRect([overlay_view_->cocoa_view() frame]));
    overlay_view_rect.set_x(overlay_view_offset_.x());
    overlay_view_rect.set_y(view_rect.height() -
                            overlay_view_rect.height() -
                            overlay_view_offset_.y());
    if (!overlay_view_->compositing_iosurface_->DrawIOSurface(
            compositing_iosurface_context_, overlay_view_rect,
            overlay_view_->ViewScaleFactor(), true)) {
      GotAcceleratedCompositingError();
      return;
    }
  }

  SendPendingLatencyInfoToHost();
}

void RenderWidgetHostViewMac::GotAcceleratedCompositingError() {
  LOG(ERROR) << "Encountered accelerated compositing error";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RenderWidgetHostViewMac::DestroyCompositingStateOnError,
                 weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostViewMac::DestroyCompositingStateOnError() {
  // This should be called with a clean stack. Make sure that no context is
  // current.
  DCHECK(!CGLGetCurrentContext());

  // The existing GL contexts may be in a bad state, so don't re-use any of the
  // existing ones anymore, rather, allocate new ones.
  if (compositing_iosurface_context_)
    compositing_iosurface_context_->PoisonContextAndSharegroup();

  DestroyCompositedIOSurfaceAndLayer(kDestroyContext);

  // Request that a new frame be generated and dirty the view.
  if (render_widget_host_)
    render_widget_host_->ScheduleComposite();
  [cocoa_view_ setNeedsDisplay:YES];

  // Mark the last frame as not accelerated (so that the window is prepared for
  // an underlay next time an accelerated frame comes in).
  last_frame_was_accelerated_ = false;

  // TODO(ccameron): It may be a good idea to request that the renderer recreate
  // its GL context as well, and fall back to software if this happens
  // repeatedly.
}

void RenderWidgetHostViewMac::SetOverlayView(
    RenderWidgetHostViewMac* overlay, const gfx::Point& offset) {
  if (overlay_view_)
    overlay_view_->underlay_view_.reset();

  overlay_view_ = overlay->overlay_view_weak_factory_.GetWeakPtr();
  overlay_view_->underlay_view_ = overlay_view_weak_factory_.GetWeakPtr();
  if (use_core_animation_)
    return;

  overlay_view_offset_ = offset;
  overlay_view_->underlay_view_has_drawn_ = false;

  [cocoa_view_ setNeedsDisplay:YES];
  [[cocoa_view_ window] disableScreenUpdatesUntilFlush];
}

void RenderWidgetHostViewMac::RemoveOverlayView() {
  if (overlay_view_) {
    overlay_view_->underlay_view_.reset();
    overlay_view_.reset();
  }
  if (use_core_animation_)
    return;

  [cocoa_view_ setNeedsDisplay:YES];
  [[cocoa_view_ window] disableScreenUpdatesUntilFlush];
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
  const size_t loop_end_idx = std::min(bounds.size(), range.end());
  int max_height = 0;
  int min_y_offset = kint32max;
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
  DCHECK(actual_range);
  DCHECK(!composition_bounds_.empty());
  DCHECK(range.start() <= composition_bounds_.size());
  DCHECK(range.end() <= composition_bounds_.size());

  if (range.is_empty()) {
    *actual_range = range;
    if (range.start() == composition_bounds_.size()) {
      return gfx::Rect(composition_bounds_[range.start() - 1].right(),
                       composition_bounds_[range.start() - 1].y(),
                       0,
                       composition_bounds_[range.start() - 1].height());
    } else {
      return gfx::Rect(composition_bounds_[range.start()].x(),
                       composition_bounds_[range.start()].y(),
                       0,
                       composition_bounds_[range.start()].height());
    }
  }

  size_t end_idx;
  if (!GetLineBreakIndex(composition_bounds_, range, &end_idx)) {
    end_idx = range.end();
  }
  *actual_range = gfx::Range(range.start(), end_idx);
  gfx::Rect rect = composition_bounds_[range.start()];
  for (size_t i = range.start() + 1; i < end_idx; ++i) {
    rect.Union(composition_bounds_[i]);
  }
  return rect;
}

gfx::Range RenderWidgetHostViewMac::ConvertCharacterRangeToCompositionRange(
    const gfx::Range& request_range) {
  if (composition_range_.is_empty())
    return gfx::Range::InvalidRange();

  if (request_range.is_reversed())
    return gfx::Range::InvalidRange();

  if (request_range.start() < composition_range_.start() ||
      request_range.start() > composition_range_.end() ||
      request_range.end() > composition_range_.end()) {
    return gfx::Range::InvalidRange();
  }

  return gfx::Range(
      request_range.start() - composition_range_.start(),
      request_range.end() - composition_range_.start());
}

WebContents* RenderWidgetHostViewMac::GetWebContents() {
  if (!render_widget_host_->IsRenderView())
    return NULL;

  return WebContents::FromRenderViewHost(
      RenderViewHost::From(render_widget_host_));
}

bool RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(
    NSRange range,
    NSRect* rect,
    NSRange* actual_range) {
  DCHECK(rect);
  // This exists to make IMEs more responsive, see http://crbug.com/115920
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::GetFirstRectForCharacterRange");

  // If requested range is same as caret location, we can just return it.
  if (selection_range_.is_empty() && gfx::Range(range) == selection_range_) {
    if (actual_range)
      *actual_range = range;
    *rect = NSRectFromCGRect(caret_rect_.ToCGRect());
    return true;
  }

  const gfx::Range request_range_in_composition =
      ConvertCharacterRangeToCompositionRange(gfx::Range(range));
  if (request_range_in_composition == gfx::Range::InvalidRange())
    return false;

  // If firstRectForCharacterRange in WebFrame is failed in renderer,
  // ImeCompositionRangeChanged will be sent with empty vector.
  if (composition_bounds_.empty())
    return false;
  DCHECK_EQ(composition_bounds_.size(), composition_range_.length());

  gfx::Range ui_actual_range;
  *rect = NSRectFromCGRect(GetFirstRectForCompositionRange(
                               request_range_in_composition,
                               &ui_actual_range).ToCGRect());
  if (actual_range) {
    *actual_range = gfx::Range(
        composition_range_.start() + ui_actual_range.start(),
        composition_range_.start() + ui_actual_range.end()).ToNSRange();
  }
  return true;
}

void RenderWidgetHostViewMac::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  TRACE_EVENT0("browser",
      "RenderWidgetHostViewMac::AcceleratedSurfaceBuffersSwapped");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AddPendingSwapAck(params.route_id,
                    gpu_host_id,
                    compositing_iosurface_ ?
                        compositing_iosurface_->GetRendererID() : 0);
  CompositorSwapBuffers(params.surface_handle,
                        params.size,
                        params.scale_factor,
                        params.latency_info);
}

void RenderWidgetHostViewMac::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  TRACE_EVENT0("browser",
      "RenderWidgetHostViewMac::AcceleratedSurfacePostSubBuffer");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AddPendingSwapAck(params.route_id,
                    gpu_host_id,
                    compositing_iosurface_ ?
                        compositing_iosurface_->GetRendererID() : 0);
  CompositorSwapBuffers(params.surface_handle,
                        params.surface_size,
                        params.surface_scale_factor,
                        params.latency_info);
}

void RenderWidgetHostViewMac::AcceleratedSurfaceSuspend() {
  if (compositing_iosurface_)
    compositing_iosurface_->UnrefIOSurface();
}

void RenderWidgetHostViewMac::AcceleratedSurfaceRelease() {
  DestroyCompositedIOSurfaceAndLayer(kDestroyContext);
}

bool RenderWidgetHostViewMac::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  if (last_frame_was_accelerated_) {
    return compositing_iosurface_ &&
           compositing_iosurface_->HasIOSurface() &&
           (desired_size.IsEmpty() ||
               compositing_iosurface_->dip_io_surface_size() == desired_size);
  } else {
    return (software_frame_manager_->HasCurrentFrame() &&
           (desired_size.IsEmpty() ||
               software_frame_manager_->GetCurrentFrameSizeInDIP() ==
                   desired_size));
  }
  return false;
}

void RenderWidgetHostViewMac::OnSwapCompositorFrame(
    uint32 output_surface_id, scoped_ptr<cc::CompositorFrame> frame) {
  // Only software compositor frames are accepted.
  if (!frame->software_frame_data) {
    DLOG(ERROR) << "Received unexpected frame type.";
    RecordAction(
        base::UserMetricsAction("BadMessageTerminate_UnexpectedFrameType"));
    render_widget_host_->GetProcess()->ReceivedBadMessage();
    return;
  }

  if (!software_frame_manager_->SwapToNewFrame(
          output_surface_id,
          frame->software_frame_data.get(),
          frame->metadata.device_scale_factor,
          render_widget_host_->GetProcess()->GetHandle())) {
    render_widget_host_->GetProcess()->ReceivedBadMessage();
    return;
  }

  // Add latency info to report when the frame finishes drawing.
  AddPendingLatencyInfo(frame->metadata.latency_info);
  GotSoftwareFrame();

  cc::CompositorFrameAck ack;
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      render_widget_host_->GetRoutingID(),
      software_frame_manager_->GetCurrentFrameOutputSurfaceId(),
      render_widget_host_->GetProcess()->GetID(),
      ack);
  software_frame_manager_->SwapToNewFrameComplete(
      !render_widget_host_->is_hidden());

  // Notify observers, tab capture observers in particular, that a new software
  // frame has come in.
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      Source<RenderWidgetHost>(render_widget_host_),
      NotificationService::NoDetails());
}

void RenderWidgetHostViewMac::OnAcceleratedCompositingStateChange() {
}

void RenderWidgetHostViewMac::AcceleratedSurfaceInitialized(int host_id,
                                                            int route_id) {
}

void RenderWidgetHostViewMac::GetScreenInfo(blink::WebScreenInfo* results) {
  *results = GetWebScreenInfo(GetNativeView());
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

gfx::GLSurfaceHandle RenderWidgetHostViewMac::GetCompositingSurface() {
  // TODO(kbr): may be able to eliminate PluginWindowHandle argument
  // completely on Mac OS.
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_TRANSPORT);
}

void RenderWidgetHostViewMac::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
}

void RenderWidgetHostViewMac::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
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

  if (render_widget_host_)
    render_widget_host_->LostMouseLock();
}

void RenderWidgetHostViewMac::HandledWheelEvent(
    const blink::WebMouseWheelEvent& event,
    bool consumed) {
  if (event.deltaX || event.deltaY)
    [cocoa_view_ gotWheelEventConsumed:consumed];
}

bool RenderWidgetHostViewMac::Send(IPC::Message* message) {
  if (render_widget_host_)
    return render_widget_host_->Send(message);
  delete message;
  return false;
}

void RenderWidgetHostViewMac::SoftwareFrameWasFreed(
    uint32 output_surface_id, unsigned frame_id) {
  if (!render_widget_host_)
    return;
  cc::CompositorFrameAck ack;
  ack.last_software_frame_id = frame_id;
  RenderWidgetHostImpl::SendReclaimCompositorResources(
      render_widget_host_->GetRoutingID(),
      output_surface_id,
      render_widget_host_->GetProcess()->GetID(),
      ack);
}

void RenderWidgetHostViewMac::ReleaseReferencesToSoftwareFrame() {
  DestroySoftwareLayer();
}

void RenderWidgetHostViewMac::ShutdownHost() {
  weak_factory_.InvalidateWeakPtrs();
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

void RenderWidgetHostViewMac::GotAcceleratedFrame() {
  EnsureCompositedIOSurfaceLayer();
  SendVSyncParametersToRenderer();
  if (!last_frame_was_accelerated_) {
    last_frame_was_accelerated_ = true;

    if (!use_core_animation_) {
      // Need to wipe the software view with transparency to expose the GL
      // underlay. Invalidate the whole window to do that.
      [cocoa_view_ setNeedsDisplay:YES];
    }

    // Delete software backingstore and layer.
    software_frame_manager_->DiscardCurrentFrame();
    DestroySoftwareLayer();
  }
}

void RenderWidgetHostViewMac::GotSoftwareFrame() {
  if (!render_widget_host_)
    return;

  EnsureSoftwareLayer();
  LayoutLayers();
  SendVSyncParametersToRenderer();

  // Draw the contents of the frame immediately. It is critical that this
  // happen before the frame be acked, otherwise the new frame will likely be
  // ready before the drawing is complete, thrashing the browser main thread.
  if (use_core_animation_) {
    [software_layer_ setNeedsDisplay];
    [software_layer_ displayIfNeeded];
  } else {
    [cocoa_view_ setNeedsDisplay:YES];
    [cocoa_view_ displayIfNeeded];
  }

  if (last_frame_was_accelerated_) {
    last_frame_was_accelerated_ = false;

    // If overlapping views are allowed, then don't unbind the context
    // from the view (that is, don't call clearDrawble -- just delete the
    // texture and IOSurface). Rather, let it sit behind the software frame
    // that will be put up in front. This will prevent transparent
    // flashes.
    // http://crbug.com/154531
    // Also note that it is necessary that clearDrawable be called if
    // overlapping views are not allowed, e.g, for content shell.
    // http://crbug.com/178408
    // Disable screen updates so that the changes of flashes is minimized.
    // http://crbug.com/279472
    if (!use_core_animation_)
      [[cocoa_view_ window] disableScreenUpdatesUntilFlush];
    if (allow_overlapping_views_)
      DestroyCompositedIOSurfaceAndLayer(kLeaveContextBoundToView);
    else
      DestroyCompositedIOSurfaceAndLayer(kDestroyContext);
  }
}

void RenderWidgetHostViewMac::TimerSinceGotAcceleratedFrameFired() {
  [compositing_iosurface_layer_ timerSinceGotNewFrameFired];
}

void RenderWidgetHostViewMac::SetActive(bool active) {
  if (render_widget_host_) {
    render_widget_host_->SetActive(active);
    if (active) {
      if (HasFocus())
        render_widget_host_->Focus();
    } else {
      render_widget_host_->Blur();
    }
  }
  if (HasFocus())
    SetTextInputActive(active);
  if (!active) {
    [cocoa_view_ setPluginImeActive:NO];
    UnlockMouse();
  }
}

void RenderWidgetHostViewMac::SetWindowVisibility(bool visible) {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_SetWindowVisibility(
        render_widget_host_->GetRoutingID(), visible));
  }
}

void RenderWidgetHostViewMac::WindowFrameChanged() {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_WindowFrameChanged(
        render_widget_host_->GetRoutingID(), GetBoundsInRootWindow(),
        GetViewBounds()));
  }

  if (compositing_iosurface_ && !use_core_animation_) {
    // This will migrate the context to the appropriate window.
    if (!EnsureCompositedIOSurface())
      GotAcceleratedCompositingError();
  }
}

void RenderWidgetHostViewMac::ShowDefinitionForSelection() {
  RenderWidgetHostViewMacDictionaryHelper helper(this);
  helper.ShowDefinitionForSelection();
}

void RenderWidgetHostViewMac::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  if (render_widget_host_)
    render_widget_host_->Send(new ViewMsg_SetBackground(
        render_widget_host_->GetRoutingID(), background));
}

void RenderWidgetHostViewMac::CreateBrowserAccessibilityManagerIfNeeded() {
  if (!GetBrowserAccessibilityManager()) {
    SetBrowserAccessibilityManager(
        new BrowserAccessibilityManagerMac(
            cocoa_view_,
            BrowserAccessibilityManagerMac::GetEmptyDocument(),
            NULL));
  }
}

void RenderWidgetHostViewMac::SetTextInputActive(bool active) {
  if (active) {
    if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD)
      EnablePasswordInput();
    else
      DisablePasswordInput();
  } else {
    if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD)
      DisablePasswordInput();
  }
}

void RenderWidgetHostViewMac::OnPluginFocusChanged(bool focused,
                                                   int plugin_id) {
  [cocoa_view_ pluginFocusChanged:(focused ? YES : NO) forPlugin:plugin_id];
}

void RenderWidgetHostViewMac::OnStartPluginIme() {
  [cocoa_view_ setPluginImeActive:YES];
}

gfx::Rect RenderWidgetHostViewMac::GetScaledOpenGLPixelRect(
    const gfx::Rect& rect) {
  gfx::Rect src_gl_subrect = rect;
  src_gl_subrect.set_y(GetViewBounds().height() - rect.bottom());

  return gfx::ToEnclosingRect(gfx::ScaleRect(src_gl_subrect,
                                             ViewScaleFactor()));
}

void RenderWidgetHostViewMac::AddPendingLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  // If a screenshot is being taken when using CoreAnimation, send a few extra
  // calls to setNeedsDisplay and wait for their resulting display calls,
  // before reporting that the frame has reached the screen.
  if (use_core_animation_) {
    bool should_defer = false;
    for (size_t i = 0; i < latency_info.size(); i++) {
      if (latency_info[i].FindLatency(
              ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT,
              render_widget_host_->GetLatencyComponentId(),
              NULL)) {
        should_defer = true;
      }
    }
    if (should_defer) {
      // Multiple pending screenshot requests will work, but if every frame
      // requests a screenshot, then the delay will never expire. Assert this
      // here to avoid this.
      CHECK_EQ(pending_latency_info_delay_, 0u);
      // Wait a fixed number of frames (calls to CALayer::display) before
      // claiming that the screenshot has reached the screen. This number
      // comes from taking the first number where tests didn't fail (six),
      // and doubling it.
      const uint32 kScreenshotLatencyDelayInFrames = 12;
      pending_latency_info_delay_ = kScreenshotLatencyDelayInFrames;
      TickPendingLatencyInfoDelay();
    }
  }

  for (size_t i = 0; i < latency_info.size(); i++) {
    pending_latency_info_.push_back(latency_info[i]);
  }
}

void RenderWidgetHostViewMac::SendPendingLatencyInfoToHost() {
  if (pending_latency_info_delay_) {
    pending_latency_info_delay_ -= 1;
    return;
  }
  pending_latency_info_delay_weak_ptr_factory_.InvalidateWeakPtrs();

  for (size_t i = 0; i < pending_latency_info_.size(); i++) {
    pending_latency_info_[i].AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0);
    render_widget_host_->FrameSwapped(pending_latency_info_[i]);
  }
  pending_latency_info_.clear();
}

void RenderWidgetHostViewMac::TickPendingLatencyInfoDelay() {
  if (compositing_iosurface_layer_) {
    // Keep calling gotNewFrame in a loop until enough display calls come in.
    // Each call will be separated by about a vsync.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RenderWidgetHostViewMac::TickPendingLatencyInfoDelay,
                   pending_latency_info_delay_weak_ptr_factory_.GetWeakPtr()));
    [compositing_iosurface_layer_ gotNewFrame];
  }
  if (software_layer_) {
    // In software mode, setNeedsDisplay will almost immediately result in the
    // layer's draw function being called, so manually insert a pretend-vsync
    // at 60 Hz.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RenderWidgetHostViewMac::TickPendingLatencyInfoDelay,
                   pending_latency_info_delay_weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(1000/60));
    [software_layer_ setNeedsDisplay];
  }
}

void RenderWidgetHostViewMac::AddPendingSwapAck(
    int32 route_id, int gpu_host_id, int32 renderer_id) {
  // Note that multiple un-acked swaps can come in the event of a GPU process
  // loss. Drop the old acks.
  pending_swap_ack_.reset(new PendingSwapAck(
      route_id, gpu_host_id, renderer_id));
}

void RenderWidgetHostViewMac::SendPendingSwapAck() {
  if (!pending_swap_ack_)
    return;

  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.sync_point = 0;
  ack_params.renderer_id = pending_swap_ack_->renderer_id;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(pending_swap_ack_->route_id,
                                                 pending_swap_ack_->gpu_host_id,
                                                 ack_params);
  pending_swap_ack_.reset();
}

void RenderWidgetHostViewMac::PauseForPendingResizeOrRepaintsAndDraw() {
  if (!render_widget_host_ || render_widget_host_->is_hidden())
    return;

  // Pausing for the overlay view prevents the underlay from receiving
  // frames. This may lead to large delays, causing overlaps. If both
  // overlay and underlay resize at the same time, let them both to have
  // some time waiting. See crbug.com/352020.
  if (underlay_view_ &&
      underlay_view_->render_widget_host_ &&
      !underlay_view_->render_widget_host_->
          CanPauseForPendingResizeOrRepaints())
    return;

  // Ensure that all frames are acked before waiting for a frame to come in.
  // Note that we will draw a frame at the end of this function, so it is safe
  // to ack a never-drawn frame here.
  SendPendingSwapAck();

  // Wait for a frame of the right size to come in.
  render_widget_host_->PauseForPendingResizeOrRepaints();

  // Immediately draw any frames that haven't been drawn yet. This is necessary
  // to keep the window and the window's contents in sync.
  [cocoa_view_ displayIfNeeded];
  [software_layer_ displayIfNeeded];
  [compositing_iosurface_layer_ displayIfNeeded];
}

void RenderWidgetHostViewMac::LayoutLayers() {
  if (!use_core_animation_)
    return;

  // Disable animation of the layer's resizing or change in contents scale.
  ScopedCAActionDisabler disabler;

  CGRect new_background_frame = NSRectToCGRect([cocoa_view() bounds]);

  // Dynamically calling setContentsScale on a CAOpenGLLayer for which
  // setAsynchronous is dynamically toggled can result in flashes of corrupt
  // content. Work around this by replacing the entire layer when the scale
  // factor changes.
  if (compositing_iosurface_ &&
      [compositing_iosurface_layer_
          respondsToSelector:(@selector(contentsScale))]) {
    if (compositing_iosurface_->scale_factor() !=
        [compositing_iosurface_layer_ contentsScale]) {
      DestroyCompositedIOSurfaceLayer(kRemoveLayerFromHierarchy);
      EnsureCompositedIOSurfaceLayer();
    }
  }
  if (compositing_iosurface_ && compositing_iosurface_layer_) {
    CGRect layer_bounds = CGRectMake(
      0,
      0,
      compositing_iosurface_->dip_io_surface_size().width(),
      compositing_iosurface_->dip_io_surface_size().height());
    CGPoint layer_position = CGPointMake(
      0,
      CGRectGetHeight(new_background_frame) - CGRectGetHeight(layer_bounds));
    bool bounds_changed = !CGRectEqualToRect(
        layer_bounds, [compositing_iosurface_layer_ bounds]);
    [compositing_iosurface_layer_ setPosition:layer_position];
    [compositing_iosurface_layer_ setBounds:layer_bounds];

    // If the bounds changed, then draw the frame immediately, to ensure that
    // content displayed is in sync with the window size.
    if (bounds_changed) {
      // Also, sometimes, especially when infobars are being removed, the
      // setNeedsDisplay calls are dropped on the floor, and stale content is
      // displayed. Calling displayIfNeeded will ensure that the right size
      // frame is drawn to the screen.
      // http://crbug.com/350817
      [compositing_iosurface_layer_ setNeedsDisplay];
      [compositing_iosurface_layer_ displayIfNeeded];
    }
  }

  // Dynamically update the software layer's contents scale to match the
  // software frame.
  if (software_frame_manager_->HasCurrentFrame() &&
      [software_layer_ respondsToSelector:(@selector(contentsScale))] &&
      [software_layer_ respondsToSelector:(@selector(setContentsScale:))]) {
    if (software_frame_manager_->GetCurrentFrameDeviceScaleFactor() !=
        [software_layer_ contentsScale]) {
      [software_layer_ setContentsScale:
          software_frame_manager_->GetCurrentFrameDeviceScaleFactor()];
    }
  }
  // Changing the software layer's bounds and position doesn't always result
  // in the layer being anchored to the top-left. Set the layer's frame
  // explicitly, since this is more reliable in practice.
  if (software_layer_) {
    bool frame_changed = !CGRectEqualToRect(
        new_background_frame, [software_layer_ frame]);
    if (frame_changed) {
      [software_layer_ setFrame:new_background_frame];
      [software_layer_ setNeedsDisplay];
    }
  }
}

SkBitmap::Config RenderWidgetHostViewMac::PreferredReadbackFormat() {
  return SkBitmap::kARGB_8888_Config;
}

}  // namespace content

// RenderWidgetHostViewCocoa ---------------------------------------------------

@implementation RenderWidgetHostViewCocoa
@synthesize selectedRange = selectedRange_;
@synthesize suppressNextEscapeKeyUp = suppressNextEscapeKeyUp_;
@synthesize markedRange = markedRange_;

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    self.acceptsTouchEvents = YES;
    editCommand_helper_.reset(new RenderWidgetHostViewMacEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    renderWidgetHostView_.reset(r);
    canBeKeyView_ = YES;
    focusedPluginIdentifier_ = -1;
    renderWidgetHostView_->backing_store_scale_factor_ =
        ScaleFactorForView(self);

    // OpenGL support:
    if ([self respondsToSelector:
        @selector(setWantsBestResolutionOpenGLSurface:)]) {
      [self setWantsBestResolutionOpenGLSurface:YES];
    }
    handlingGlobalFrameDidChange_ = NO;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(globalFrameDidChange:)
               name:NSViewGlobalFrameDidChangeNotification
             object:self];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didChangeScreenParameters:)
               name:NSApplicationDidChangeScreenParametersNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  // Unbind the GL context from this view. If this is not done before super's
  // dealloc is called then the GL context will crash when it reaches into
  // the view in its destructor.
  // http://crbug.com/255608
  if (renderWidgetHostView_)
    renderWidgetHostView_->AcceleratedSurfaceRelease();

  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(viewGone:)])
    [responderDelegate_ viewGone:self];
  responderDelegate_.reset();

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

- (void)didChangeScreenParameters:(NSNotification*)notify {
  g_screen_info_up_to_date = false;
}

- (void)setResponderDelegate:
            (NSObject<RenderWidgetHostViewMacDelegate>*)delegate {
  DCHECK(!responderDelegate_);
  responderDelegate_.reset([delegate retain]);
}

- (void)resetCursorRects {
  if (currentCursor_) {
    [self addCursorRect:[self visibleRect] cursor:currentCursor_];
    [currentCursor_ setOnMouseEntered:YES];
  }
}

- (void)gotWheelEventConsumed:(BOOL)consumed {
  [responderDelegate_ gotWheelEventConsumed:consumed];
}

- (BOOL)respondsToSelector:(SEL)selector {
  // Trickiness: this doesn't mean "does this object's superclass respond to
  // this selector" but rather "does the -respondsToSelector impl from the
  // superclass say that this class responds to the selector".
  if ([super respondsToSelector:selector])
    return YES;

  if (responderDelegate_)
    return [responderDelegate_ respondsToSelector:selector];

  return NO;
}

- (id)forwardingTargetForSelector:(SEL)selector {
  if ([responderDelegate_ respondsToSelector:selector])
    return responderDelegate_.get();

  return [super forwardingTargetForSelector:selector];
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (BOOL)acceptsMouseEventsWhenInactive {
  // Some types of windows (balloons, always-on-top panels) want to accept mouse
  // clicks w/o the first click being treated as 'activation'. Same applies to
  // mouse move events.
  return [[self window] level] > NSNormalWindowLevel;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return [self acceptsMouseEventsWhenInactive];
}

- (void)setTakesFocusOnlyOnMouseDown:(BOOL)b {
  takesFocusOnlyOnMouseDown_ = b;
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (BOOL)shouldIgnoreMouseEvent:(NSEvent*)theEvent {
  NSWindow* window = [self window];
  // If this is a background window, don't handle mouse movement events. This
  // is the expected behavior on the Mac as evidenced by other applications.
  if ([theEvent type] == NSMouseMoved &&
      ![self acceptsMouseEventsWhenInactive] &&
      ![window isKeyWindow]) {
    return YES;
  }

  // Use hitTest to check whether the mouse is over a nonWebContentView - in
  // which case the mouse event should not be handled by the render host.
  const SEL nonWebContentViewSelector = @selector(nonWebContentView);
  NSView* contentView = [window contentView];
  NSView* view = [contentView hitTest:[theEvent locationInWindow]];
  // Traverse the superview hierarchy as the hitTest will return the frontmost
  // view, such as an NSTextView, while nonWebContentView may be specified by
  // its parent view.
  while (view) {
    if ([view respondsToSelector:nonWebContentViewSelector] &&
        [view performSelector:nonWebContentViewSelector]) {
      // The cursor is over a nonWebContentView - ignore this mouse event.
      return YES;
    }
    if ([view isKindOfClass:[self class]] && ![view isEqual:self] &&
        !hasOpenMouseDown_) {
      // The cursor is over an overlapping render widget. This check is done by
      // both views so the one that's returned by -hitTest: will end up
      // processing the event.
      // Note that while dragging, we only get events for the render view where
      // drag started, even if mouse is  actually over another view or outside
      // the window. Cocoa does this for us. We should handle these events and
      // not ignore (since there is no other render view to handle them). Thus
      // the |!hasOpenMouseDown_| check above.
      return YES;
    }
    view = [view superview];
  }
  return NO;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::mouseEvent");
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return;
  }

  if ([self shouldIgnoreMouseEvent:theEvent]) {
    // If this is the first such event, send a mouse exit to the host view.
    if (!mouseEventWasIgnored_ && renderWidgetHostView_->render_widget_host_) {
      WebMouseEvent exitEvent =
          WebInputEventFactory::mouseEvent(theEvent, self);
      exitEvent.type = WebInputEvent::MouseLeave;
      exitEvent.button = WebMouseEvent::ButtonNone;
      renderWidgetHostView_->ForwardMouseEvent(exitEvent);
    }
    mouseEventWasIgnored_ = YES;
    return;
  }

  if (mouseEventWasIgnored_) {
    // If this is the first mouse event after a previous event that was ignored
    // due to the hitTest, send a mouse enter event to the host view.
    if (renderWidgetHostView_->render_widget_host_) {
      WebMouseEvent enterEvent =
          WebInputEventFactory::mouseEvent(theEvent, self);
      enterEvent.type = WebInputEvent::MouseMove;
      enterEvent.button = WebMouseEvent::ButtonNone;
      renderWidgetHostView_->ForwardMouseEvent(enterEvent);
    }
  }
  mouseEventWasIgnored_ = NO;

  // TODO(rohitrao): Probably need to handle other mouse down events here.
  if ([theEvent type] == NSLeftMouseDown && takesFocusOnlyOnMouseDown_) {
    if (renderWidgetHostView_->render_widget_host_)
      renderWidgetHostView_->render_widget_host_->OnPointerEventActivate();

    // Manually take focus after the click but before forwarding it to the
    // renderer.
    [[self window] makeFirstResponder:self];
  }

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

  const WebMouseEvent event =
      WebInputEventFactory::mouseEvent(theEvent, self);
  renderWidgetHostView_->ForwardMouseEvent(event);
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
  // it, we let the WebContentsView figure out how to reinject it.
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
  // equivalent that Cocoa uses for toggling the input language. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return kEventHandled;
  }

  [self keyEvent:theEvent wasKeyEquivalent:NO];
  return kEventHandled;
}

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::keyEvent");
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

  RenderWidgetHostImpl* widgetHost = renderWidgetHostView_->render_widget_host_;
  DCHECK(widgetHost);

  NativeWebKeyboardEvent event(theEvent);

  // Force fullscreen windows to close on Escape so they won't keep the keyboard
  // grabbed or be stuck onscreen if the renderer is hanging.
  if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE &&
      renderWidgetHostView_->pepper_fullscreen_window()) {
    RenderWidgetHostViewMac* parent =
        renderWidgetHostView_->fullscreen_parent_host_view();
    if (parent)
      parent->cocoa_view()->suppressNextEscapeKeyUp_ = YES;
    widgetHost->Shutdown();
    return;
  }

  // Suppress the escape key up event if necessary.
  if (event.windowsKeyCode == ui::VKEY_ESCAPE && suppressNextEscapeKeyUp_) {
    if (event.type == NativeWebKeyboardEvent::KeyUp)
      suppressNextEscapeKeyUp_ = NO;
    return;
  }

  // We only handle key down events and just simply forward other events.
  if ([theEvent type] != NSKeyDown) {
    widgetHost->ForwardKeyboardEvent(event);

    // Possibly autohide the cursor.
    if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
      [NSCursor setHiddenUntilMouseMoves:YES];

    return;
  }

  base::scoped_nsobject<RenderWidgetHostViewCocoa> keepSelfAlive([self retain]);

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

  // Before doing anything with a key down, check to see if plugin IME has been
  // cancelled, since the plugin host needs to be informed of that before
  // receiving the keydown.
  if ([theEvent type] == NSKeyDown)
    [self checkForPluginImeCancellation];

  // Sends key down events to input method first, then we can decide what should
  // be done according to input method's feedback.
  // If a plugin is active, bypass this step since events are forwarded directly
  // to the plugin IME.
  if (focusedPluginIdentifier_ == -1)
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
    if (!editCommands_.empty()) {
      widgetHost->Send(new InputMsg_SetEditCommandsForNextKeyEvent(
          widgetHost->GetRoutingID(), editCommands_));
    }
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
    widgetHost->ImeConfirmComposition(
        textToBeInserted_, gfx::Range::InvalidRange(), false);
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
    if (unmarkTextCalled_) {
      widgetHost->ImeConfirmComposition(
          base::string16(), gfx::Range::InvalidRange(), false);
    } else {
      widgetHost->ImeCancelComposition();
    }
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
    fakeEvent.type = blink::WebInputEvent::KeyUp;
    fakeEvent.skip_in_browser = true;
    widgetHost->ForwardKeyboardEvent(fakeEvent);
    // Not checking |renderWidgetHostView_->render_widget_host_| here because
    // a key event with |skip_in_browser| == true won't be handled by browser,
    // thus it won't destroy the widget.

    if (!editCommands_.empty()) {
      widgetHost->Send(new InputMsg_SetEditCommandsForNextKeyEvent(
          widgetHost->GetRoutingID(), editCommands_));
    }
    widgetHost->ForwardKeyboardEvent(event);

    // Calling ForwardKeyboardEvent() could have destroyed the widget. When the
    // widget was destroyed, |renderWidgetHostView_->render_widget_host_| will
    // be set to NULL. So we check it here and return immediately if it's NULL.
    if (!renderWidgetHostView_->render_widget_host_)
      return;
  }

  const NSUInteger kCtrlCmdKeyMask = NSControlKeyMask | NSCommandKeyMask;
  // Only send a corresponding key press event if there is no marked text.
  if (!hasMarkedText_) {
    if (!textInserted && textToBeInserted_.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      event.type = blink::WebInputEvent::Char;
      event.text[0] = textToBeInserted_[0];
      event.text[1] = 0;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEvent(event);
    } else if ((!textInserted || delayEventUntilAfterImeCompostion) &&
               [[theEvent characters] length] > 0 &&
               (([theEvent modifierFlags] & kCtrlCmdKeyMask) ||
                (hasEditCommands_ && editCommands_.empty()))) {
      // We don't get insertText: calls if ctrl or cmd is down, or the key event
      // generates an insert command. So synthesize a keypress event for these
      // cases, unless the key event generated any other command.
      event.type = blink::WebInputEvent::Char;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEvent(event);
    }
  }

  // Possibly autohide the cursor.
  if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
    [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)shortCircuitScrollWheelEvent:(NSEvent*)event {
  DCHECK(base::mac::IsOSLionOrLater());

  if ([event phase] != NSEventPhaseEnded &&
      [event phase] != NSEventPhaseCancelled) {
    return;
  }

  if (renderWidgetHostView_->render_widget_host_) {
    // History-swiping is not possible if the logic reaches this point.
    // Allow rubber-banding in both directions.
    bool canRubberbandLeft = true;
    bool canRubberbandRight = true;
    const WebMouseWheelEvent webEvent = WebInputEventFactory::mouseWheelEvent(
        event, self, canRubberbandLeft, canRubberbandRight);
    renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(webEvent);
  }

  if (endWheelMonitor_) {
    [NSEvent removeMonitor:endWheelMonitor_];
    endWheelMonitor_ = nil;
  }
}

- (void)beginGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ beginGestureWithEvent:event];
}
- (void)endGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ endGestureWithEvent:event];
}
- (void)touchesMovedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesMovedWithEvent:event];
}
- (void)touchesBeganWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesBeganWithEvent:event];
}
- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesCancelledWithEvent:event];
}
- (void)touchesEndedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesEndedWithEvent:event];
}

// This is invoked only on 10.8 or newer when the user taps a word using
// three fingers.
- (void)quickLookWithEvent:(NSEvent*)event {
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      renderWidgetHostView_->render_widget_host_,
      gfx::Point(point.x, NSHeight([self frame]) - point.y),
      ^(NSAttributedString* string, NSPoint baselinePoint) {
          if (string && [string length] > 0) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self showDefinitionForAttributedString:string
                                                atPoint:baselinePoint];
            });
          }
      }
  );
}

// This method handles 2 different types of hardware events.
// (Apple does not distinguish between them).
//  a. Scrolling the middle wheel of a mouse.
//  b. Swiping on the track pad.
//
// This method is responsible for 2 types of behavior:
//  a. Scrolling the content of window.
//  b. Navigating forwards/backwards in history.
//
// This is a brief description of the logic:
//  1. If the content can be scrolled, scroll the content.
//     (This requires a roundtrip to blink to determine whether the content
//      can be scrolled.)
//     Once this logic is triggered, the navigate logic cannot be triggered
//     until the gesture finishes.
//  2. If the user is making a horizontal swipe, start the navigate
//     forward/backwards UI.
//     Once this logic is triggered, the user can either cancel or complete
//     the gesture. If the user completes the gesture, all remaining touches
//     are swallowed, and not allowed to scroll the content. If the user
//     cancels the gesture, all remaining touches are forwarded to the content
//     scroll logic. The user cannot trigger the navigation logic again.
- (void)scrollWheel:(NSEvent*)event {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:event];
    if (handled)
      return;
  }

  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the view
  // when the scrolling ends (e.g. if the tab was switched). This is necessary
  // for ending rubber-banding in such cases.
  if (base::mac::IsOSLionOrLater() && [event phase] == NSEventPhaseBegan &&
      !endWheelMonitor_) {
    endWheelMonitor_ =
      [NSEvent addLocalMonitorForEventsMatchingMask:NSScrollWheelMask
      handler:^(NSEvent* blockEvent) {
          [self shortCircuitScrollWheelEvent:blockEvent];
          return blockEvent;
      }];
  }

  // This is responsible for content scrolling!
  if (renderWidgetHostView_->render_widget_host_) {
    BOOL canRubberbandLeft = [responderDelegate_ canRubberbandLeft:self];
    BOOL canRubberbandRight = [responderDelegate_ canRubberbandRight:self];
    const WebMouseWheelEvent webEvent = WebInputEventFactory::mouseWheelEvent(
        event, self, canRubberbandLeft, canRubberbandRight);
    renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(webEvent);
  }
}

// Called repeatedly during a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
  if (renderWidgetHostView_->render_widget_host_) {
    // Send a GesturePinchUpdate event.
    // Note that we don't attempt to bracket these by GesturePinchBegin/End (or
    // GestureSrollBegin/End) as is done for touchscreen.  Keeping track of when
    // a pinch is active would take a little more work here, and we don't need
    // it for anything yet.
    const WebGestureEvent& webEvent =
        WebInputEventFactory::gestureEvent(event, self);
    renderWidgetHostView_->render_widget_host_->ForwardGestureEvent(webEvent);
  }
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSWindow* oldWindow = [self window];

  // We're messing with the window, so do this to ensure no flashes. This one
  // prevents a flash when the current tab is closed.
  if (!renderWidgetHostView_->use_core_animation_)
    [oldWindow disableScreenUpdatesUntilFlush];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  // Backing property notifications crash on 10.6 when building with the 10.7
  // SDK, see http://crbug.com/260595.
  static BOOL supportsBackingPropertiesNotification =
      SupportsBackingPropertiesChangedNotification();

  if (oldWindow) {
    if (supportsBackingPropertiesNotification) {
      [notificationCenter
          removeObserver:self
                    name:NSWindowDidChangeBackingPropertiesNotification
                  object:oldWindow];
    }
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidMoveNotification
                object:oldWindow];
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidEndLiveResizeNotification
                object:oldWindow];
  }
  if (newWindow) {
    if (supportsBackingPropertiesNotification) {
      [notificationCenter
          addObserver:self
             selector:@selector(windowDidChangeBackingProperties:)
                 name:NSWindowDidChangeBackingPropertiesNotification
               object:newWindow];
    }
    [notificationCenter
        addObserver:self
           selector:@selector(windowChangedGlobalFrame:)
               name:NSWindowDidMoveNotification
             object:newWindow];
    [notificationCenter
        addObserver:self
           selector:@selector(windowChangedGlobalFrame:)
               name:NSWindowDidEndLiveResizeNotification
             object:newWindow];
  }
}

- (void)updateScreenProperties{
  renderWidgetHostView_->UpdateBackingStoreScaleFactor();
  renderWidgetHostView_->UpdateDisplayLink();
}

// http://developer.apple.com/library/mac/#documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/CapturingScreenContents/CapturingScreenContents.html#//apple_ref/doc/uid/TP40012302-CH10-SW4
- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  // Background tabs check if their scale factor or vsync properties changed
  // when they are added to a window.

  // Allocating a CGLayerRef with the current scale factor immediately from
  // this handler doesn't work. Schedule the backing store update on the
  // next runloop cycle, then things are read for CGLayerRef allocations to
  // work.
  [self performSelector:@selector(updateScreenProperties)
             withObject:nil
             afterDelay:0];
}

- (void)globalFrameDidChange:(NSNotification*)notification {
  if (handlingGlobalFrameDidChange_)
    return;

  handlingGlobalFrameDidChange_ = YES;
  if (!renderWidgetHostView_->use_core_animation_ &&
      renderWidgetHostView_->compositing_iosurface_context_) {
    [renderWidgetHostView_->compositing_iosurface_context_->nsgl_context()
        update];
  }
  handlingGlobalFrameDidChange_ = NO;
}

- (void)windowChangedGlobalFrame:(NSNotification*)notification {
  renderWidgetHostView_->UpdateScreenInfo(
      renderWidgetHostView_->GetNativeView());
}

- (void)setFrameSize:(NSSize)newSize {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::setFrameSize");

  // NB: -[NSView setFrame:] calls through -setFrameSize:, so overriding
  // -setFrame: isn't neccessary.
  [super setFrameSize:newSize];

  if (!renderWidgetHostView_->render_widget_host_)
    return;

  // Move the CALayers to their positions in the new view size. Note that
  // this will not draw anything because the non-background layers' sizes
  // didn't actually change.
  renderWidgetHostView_->LayoutLayers();

  renderWidgetHostView_->render_widget_host_->SendScreenRects();
  renderWidgetHostView_->render_widget_host_->WasResized();

  // Wait for the frame that WasResize might have requested. If the view is
  // being made visible at a new size, then this call will have no effect
  // because the view widget is still hidden, and the pause call in WasShown
  // will have this effect for us.
  renderWidgetHostView_->PauseForPendingResizeOrRepaintsAndDraw();
}

// Fills with white the parts of the area to the right and bottom for |rect|
// that intersect |damagedRect|.
- (void)fillBottomRightRemainderOfRect:(gfx::Rect)rect
                             dirtyRect:(gfx::Rect)damagedRect
                             inContext:(CGContextRef)context {
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
    CGContextSetFillColorWithColor(context,
                                   CGColorGetConstantColor(kCGColorWhite));
    CGContextFillRect(context, NSRectToCGRect(r));
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
    CGContextSetFillColorWithColor(context,
                                   CGColorGetConstantColor(kCGColorWhite));
    CGContextFillRect(context, NSRectToCGRect(r));
  }
}

- (void)drawRect:(NSRect)dirtyRect {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::drawRect");
  DCHECK(!renderWidgetHostView_->use_core_animation_);

  if (!renderWidgetHostView_->render_widget_host_) {
    // When using CoreAnimation, this path is used to paint the contents area
    // white before any frames come in. When layers to draw frames exist, this
    // is not hit.
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    return;
  }

  const gfx::Rect damagedRect([self flipNSRectToRect:dirtyRect]);

  if (renderWidgetHostView_->last_frame_was_accelerated_ &&
      renderWidgetHostView_->compositing_iosurface_) {
    if (renderWidgetHostView_->allow_overlapping_views_) {
      // If overlapping views need to be allowed, punch a hole in the window
      // to expose the GL underlay.
      TRACE_EVENT2("gpu", "NSRectFill clear", "w", damagedRect.width(),
                   "h", damagedRect.height());
      // NSRectFill is extremely slow (15ms for a window on a fast MacPro), so
      // this is only done when it's a real invalidation from window damage (not
      // when a BuffersSwapped was received). Note that even a 1x1 NSRectFill
      // can take many milliseconds sometimes (!) so this is skipped completely
      // for drawRects that are triggered by BuffersSwapped messages.
      [[NSColor clearColor] set];
      NSRectFill(dirtyRect);
    }

    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
        renderWidgetHostView_->compositing_iosurface_context_->cgl_context());
    renderWidgetHostView_->DrawIOSurfaceWithoutCoreAnimation();
    return;
  }

  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  [self drawWithDirtyRect:NSRectToCGRect(dirtyRect)
                inContext:context];
}

- (void)drawWithDirtyRect:(CGRect)dirtyRect
                inContext:(CGContextRef)context {
  content::SoftwareFrameManager* software_frame_manager =
      renderWidgetHostView_->software_frame_manager_.get();
  if (software_frame_manager->HasCurrentFrame()) {
    // Note: All coordinates are in view units, not pixels.
    gfx::Rect bitmapRect(
            software_frame_manager->GetCurrentFrameSizeInDIP());

    // Specify the proper y offset to ensure that the view is rooted to the
    // upper left corner.  This can be negative, if the window was resized
    // smaller and the renderer hasn't yet repainted.
    int yOffset = NSHeight([self bounds]) - bitmapRect.height();

    NSRect nsDirtyRect = NSRectFromCGRect(dirtyRect);
    const gfx::Rect damagedRect([self flipNSRectToRect:nsDirtyRect]);

    gfx::Rect paintRect = gfx::IntersectRects(bitmapRect, damagedRect);
    if (!paintRect.IsEmpty()) {
      gfx::Size sizeInPixels =
          software_frame_manager->GetCurrentFrameSizeInPixels();
      base::ScopedCFTypeRef<CGDataProviderRef> dataProvider(
          CGDataProviderCreateWithData(
              NULL,
              software_frame_manager->GetCurrentFramePixels(),
              4 * sizeInPixels.width() * sizeInPixels.height(),
              NULL));
      base::ScopedCFTypeRef<CGImageRef> image(
          CGImageCreate(
              sizeInPixels.width(),
              sizeInPixels.height(),
              8,
              32,
              4 * sizeInPixels.width(),
              base::mac::GetSystemColorSpace(),
              kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
              dataProvider,
              NULL,
              false,
              kCGRenderingIntentDefault));
      CGRect imageRect = bitmapRect.ToCGRect();
      imageRect.origin.y = yOffset;
      CGContextDrawImage(context, imageRect, image);
    }

    renderWidgetHostView_->SendPendingLatencyInfoToHost();

    // Fill the remaining portion of the damagedRect with white
    [self fillBottomRightRemainderOfRect:bitmapRect
                               dirtyRect:damagedRect
                               inContext:context];

    if (!renderWidgetHostView_->whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks();
    }
    if (!renderWidgetHostView_->web_contents_switch_paint_time_.is_null()) {
      base::TimeDelta web_contents_switch_paint_duration =
          base::TimeTicks::Now() -
              renderWidgetHostView_->web_contents_switch_paint_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration",
          web_contents_switch_paint_duration);
      // Reset contents_switch_paint_time_ to 0 so future tab selections are
      // recorded.
      renderWidgetHostView_->web_contents_switch_paint_time_ =
          base::TimeTicks();
    }
  } else {
    CGContextSetFillColorWithColor(context,
                                   CGColorGetConstantColor(kCGColorWhite));
    CGContextFillRect(context, dirtyRect);
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

  return canBeKeyView_ && !takesFocusOnlyOnMouseDown_;
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

  NSNumber* direction = [NSNumber numberWithUnsignedInteger:
      [[self window] keyViewSelectionDirection]];
  NSDictionary* userInfo =
      [NSDictionary dictionaryWithObject:direction
                                  forKey:kSelectionDirection];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kViewDidBecomeFirstResponder
                    object:self
                  userInfo:userInfo];

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
  if (responderDelegate_ &&
      [responderDelegate_
          respondsToSelector:@selector(validateUserInterfaceItem:
                                                     isValidItem:)]) {
    BOOL valid;
    BOOL known =
        [responderDelegate_ validateUserInterfaceItem:item isValidItem:&valid];
    if (known)
      return valid;
  }

  SEL action = [item action];

  if (action == @selector(stopSpeaking:)) {
    return renderWidgetHostView_->render_widget_host_->IsRenderView() &&
           renderWidgetHostView_->IsSpeaking();
  }
  if (action == @selector(startSpeaking:)) {
    return renderWidgetHostView_->render_widget_host_->IsRenderView() &&
           renderWidgetHostView_->SupportsSpeech();
  }

  // For now, these actions are always enabled for render view,
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(undo:) ||
      action == @selector(redo:) ||
      action == @selector(cut:) ||
      action == @selector(copy:) ||
      action == @selector(copyToFindPboard:) ||
      action == @selector(paste:) ||
      action == @selector(pasteAndMatchStyle:)) {
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
  return ([event type] == NSKeyDown &&
             !([event modifierFlags] & NSCommandKeyMask)) ? YES : NO;
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
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->GetBrowserAccessibilityManager();

  // Contents specifies document view of RenderWidgetHostViewCocoa provided by
  // BrowserAccessibilityManager. Children includes all subviews in addition to
  // contents. Currently we do not have subviews besides the document view.
  if (([attribute isEqualToString:NSAccessibilityChildrenAttribute] ||
          [attribute isEqualToString:NSAccessibilityContentsAttribute]) &&
      manager) {
    return [NSArray arrayWithObjects:manager->
        GetRoot()->ToBrowserAccessibilityCocoa(), nil];
  } else if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return NSAccessibilityScrollAreaRole;
  }
  id ret = [super accessibilityAttributeValue:attribute];
  return ret;
}

- (NSArray*)accessibilityAttributeNames {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [ret addObject:NSAccessibilityContentsAttribute];
  [ret addObjectsFromArray:[super accessibilityAttributeNames]];
  return ret;
}

- (id)accessibilityHitTest:(NSPoint)point {
  if (!renderWidgetHostView_->GetBrowserAccessibilityManager())
    return self;
  NSPoint pointInWindow = [[self window] convertScreenToBase:point];
  NSPoint localPoint = [self convertPoint:pointInWindow fromView:nil];
  localPoint.y = NSHeight([self bounds]) - localPoint.y;
  BrowserAccessibilityCocoa* root = renderWidgetHostView_->
      GetBrowserAccessibilityManager()->
          GetRoot()->ToBrowserAccessibilityCocoa();
  id obj = [root accessibilityHitTest:localPoint];
  return obj;
}

- (BOOL)accessibilityIsIgnored {
  return !renderWidgetHostView_->GetBrowserAccessibilityManager();
}

- (NSUInteger)accessibilityGetIndexOf:(id)child {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->GetBrowserAccessibilityManager();
  // Only child is root.
  if (manager &&
      manager->GetRoot()->ToBrowserAccessibilityCocoa() == child) {
    return 0;
  } else {
    return NSNotFound;
  }
}

- (id)accessibilityFocusedUIElement {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->GetBrowserAccessibilityManager();
  if (manager) {
    BrowserAccessibility* focused_item = manager->GetFocus(NULL);
    DCHECK(focused_item);
    if (focused_item) {
      BrowserAccessibilityCocoa* focused_item_cocoa =
          focused_item->ToBrowserAccessibilityCocoa();
      DCHECK(focused_item_cocoa);
      if (focused_item_cocoa)
        return focused_item_cocoa;
    }
  }
  return [super accessibilityFocusedUIElement];
}

- (void)doDefaultAction:(int32)accessibilityObjectId {
  RenderWidgetHostImpl* rwh = renderWidgetHostView_->render_widget_host_;
  rwh->Send(new AccessibilityMsg_DoDefaultAction(
      rwh->GetRoutingID(), accessibilityObjectId));
}

// VoiceOver uses this method to move the caret to the beginning of the next
// word in a text field.
- (void)accessibilitySetTextSelection:(int32)accId
                          startOffset:(int32)startOffset
                            endOffset:(int32)endOffset {
  RenderWidgetHostImpl* rwh = renderWidgetHostView_->render_widget_host_;
  rwh->AccessibilitySetTextSelection(accId, startOffset, endOffset);
}

// Convert a web accessibility's location in web coordinates into a cocoa
// screen coordinate.
- (NSPoint)accessibilityPointInScreen:(NSPoint)origin
                                 size:(NSSize)size {
  origin.y = NSHeight([self bounds]) - origin.y;
  NSPoint originInWindow = [self convertPoint:origin toView:nil];
  NSPoint originInScreen = [[self window] convertBaseToScreen:originInWindow];
  originInScreen.y = originInScreen.y - size.height;
  return originInScreen;
}

- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId {
  if (focus) {
    RenderWidgetHostImpl* rwh = renderWidgetHostView_->render_widget_host_;
    rwh->Send(new AccessibilityMsg_SetFocus(
        rwh->GetRoutingID(), accessibilityObjectId));

    // Immediately set the focused item even though we have not officially set
    // focus on it as VoiceOver expects to get the focused item after this
    // method returns.
    BrowserAccessibilityManager* manager =
        renderWidgetHostView_->GetBrowserAccessibilityManager();
    manager->SetFocus(manager->GetFromRendererID(accessibilityObjectId), false);
  }
}

- (void)performShowMenuAction:(BrowserAccessibilityCocoa*)accessibility {
  // Performs a right click copying WebKit's
  // accessibilityPerformShowMenuAction.
  NSPoint origin = [accessibility origin];
  NSSize size = [[accessibility size] sizeValue];
  NSPoint location = [self accessibilityPointInScreen:origin size:size];
  location = [[self window] convertScreenToBase:location];
  location.x += size.width/2;
  location.y += size.height/2;

  NSEvent* fakeRightClick = [NSEvent
                           mouseEventWithType:NSRightMouseDown
                                     location:location
                                modifierFlags:0
                                    timestamp:0
                                 windowNumber:[[self window] windowNumber]
                                      context:[NSGraphicsContext currentContext]
                                  eventNumber:0
                                   clickCount:1
                                     pressure:0];

  [self mouseEvent:fakeRightClick];
}

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
  NSEvent* fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
                                              location:NSZeroPoint
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
  NSEvent* fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
                                              location:NSZeroPoint
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

// Below is our NSTextInputClient implementation.
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
  DCHECK([self window]);
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.
  thePoint = [[self window] convertScreenToBase:thePoint];
  thePoint = [self convertPoint:thePoint fromView:nil];
  thePoint.y = NSHeight([self frame]) - thePoint.y;

  NSUInteger index =
      TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
          renderWidgetHostView_->render_widget_host_,
          gfx::Point(thePoint.x, thePoint.y));
  return index;
}

- (NSRect)firstViewRectForCharacterRange:(NSRange)theRange
                             actualRange:(NSRangePointer)actualRange {
  NSRect rect;
  if (!renderWidgetHostView_->GetCachedFirstRectForCharacterRange(
          theRange,
          &rect,
          actualRange)) {
    rect = TextInputClientMac::GetInstance()->GetFirstRectForRange(
        renderWidgetHostView_->render_widget_host_, theRange);

    // TODO(thakis): Pipe |actualRange| through TextInputClientMac machinery.
    if (actualRange)
      *actualRange = theRange;
  }

  // The returned rectangle is in WebKit coordinates (upper left origin), so
  // flip the coordinate system.
  NSRect viewFrame = [self frame];
  rect.origin.y = NSHeight(viewFrame) - NSMaxY(rect);
  return rect;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer)actualRange {
  NSRect rect = [self firstViewRectForCharacterRange:theRange
                                         actualRange:actualRange];

  // Convert into screen coordinates for return.
  rect = [self convertRect:rect toView:nil];
  rect.origin = [[self window] convertBaseToScreen:rect.origin];
  return rect;
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

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
    actualRange:(NSRangePointer)actualRange {
  // TODO(thakis): Pipe |actualRange| through TextInputClientMac machinery.
  if (actualRange)
    *actualRange = range;
  NSAttributedString* str =
      TextInputClientMac::GetInstance()->GetAttributedSubstringFromRange(
          renderWidgetHostView_->render_widget_host_, range);
  return str;
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

// Each RenderWidgetHostViewCocoa has its own input context, but we return
// nil when the caret is in non-editable content or password box to avoid
// making input methods do their work.
- (NSTextInputContext *)inputContext {
  if (focusedPluginIdentifier_ != -1)
    return [[ComplexTextInputPanel sharedComplexTextInputPanel] inputContext];

  switch(renderWidgetHostView_->text_input_type_) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
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
  if (!handlingKeyDown_) {
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::string16(), gfx::Range::InvalidRange(), false);
  } else {
    unmarkTextCalled_ = YES;
  }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
                              replacementRange:(NSRange)replacementRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];

  // |markedRange_| will get set on a callback from ImeSetComposition().
  selectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);

  underlines_.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &underlines_);
  } else {
    // Use a thin black underline by default.
    underlines_.push_back(
        blink::WebCompositionUnderline(0, length, SK_ColorBLACK, false));
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
      [RenderWidgetHostViewMacEditCommandHelper::
          CommandNameForSelector(selector) UTF8String]);

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
    RenderWidgetHostImpl* rwh = renderWidgetHostView_->render_widget_host_;
    rwh->Send(new InputMsg_ExecuteEditCommand(rwh->GetRoutingID(),
                                              command, ""));
  }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
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
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
  } else {
    gfx::Range replacement_range(replacementRange);
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::SysNSStringToUTF16(im_text), replacement_range, false);
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

- (void)insertText:(id)string {
  [self insertText:string replacementRange:NSMakeRange(NSNotFound, 0)];
}

- (void)viewDidMoveToWindow {
  if ([self window])
    [self updateScreenProperties];

  if (canBeKeyView_) {
    NSWindow* newWindow = [self window];
    // Pointer comparison only, since we don't know if lastWindow_ is still
    // valid.
    if (newWindow) {
      // If we move into a new window, refresh the frame information. We
      // don't need to do it if it was the same window as it used to be in,
      // since that case is covered by WasShown(). We only want to do this for
      // real browser views, not popups.
      if (newWindow != lastWindow_) {
        lastWindow_ = newWindow;
        renderWidgetHostView_->WindowFrameChanged();
      }
    }
  }

  // If we switch windows (or are removed from the view hierarchy), cancel any
  // open mouse-downs.
  if (hasOpenMouseDown_) {
    WebMouseEvent event;
    event.type = WebInputEvent::MouseUp;
    event.button = WebMouseEvent::ButtonLeft;
    renderWidgetHostView_->ForwardMouseEvent(event);

    hasOpenMouseDown_ = NO;
  }
}

- (void)undo:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Undo();
}

- (void)redo:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Redo();
}

- (void)cut:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Cut();
}

- (void)copy:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Copy();
}

- (void)copyToFindPboard:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->CopyToFindPboard();
}

- (void)paste:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Paste();
}

- (void)pasteAndMatchStyle:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->PasteAndMatchStyle();
}

- (void)selectAll:(id)sender {
  // editCommand_helper_ adds implementations for most NSResponder methods
  // dynamically. But the renderer side only sends selection results back to
  // the browser if they were triggered by a keyboard event or went through
  // one of the Select methods on RWH. Since selectAll: is called from the
  // menu handler, neither is true.
  // Explicitly call SelectAll() here to make sure the renderer returns
  // selection results.
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->SelectAll();
}

- (void)startSpeaking:(id)sender {
  renderWidgetHostView_->SpeakSelection();
}

- (void)stopSpeaking:(id)sender {
  renderWidgetHostView_->StopSpeaking();
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
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::string16(), gfx::Range::InvalidRange(), false);

  [self cancelComposition];
}

- (void)setPluginImeActive:(BOOL)active {
  if (active == pluginImeActive_)
    return;

  pluginImeActive_ = active;
  if (!active) {
    [[ComplexTextInputPanel sharedComplexTextInputPanel] cancelComposition];
    renderWidgetHostView_->PluginImeCompositionCompleted(
        base::string16(), focusedPluginIdentifier_);
  }
}

- (void)pluginFocusChanged:(BOOL)focused forPlugin:(int)pluginId {
  if (focused)
    focusedPluginIdentifier_ = pluginId;
  else if (focusedPluginIdentifier_ == pluginId)
    focusedPluginIdentifier_ = -1;

  // Whenever plugin focus changes, plugin IME resets.
  [self setPluginImeActive:NO];
}

- (BOOL)postProcessEventForPluginIme:(NSEvent*)event {
  if (!pluginImeActive_)
    return false;

  ComplexTextInputPanel* inputPanel =
      [ComplexTextInputPanel sharedComplexTextInputPanel];
  NSString* composited_string = nil;
  BOOL handled = [inputPanel interpretKeyEvent:event
                                        string:&composited_string];
  if (composited_string) {
    renderWidgetHostView_->PluginImeCompositionCompleted(
        base::SysNSStringToUTF16(composited_string), focusedPluginIdentifier_);
    pluginImeActive_ = NO;
  }
  return handled;
}

- (void)checkForPluginImeCancellation {
  if (pluginImeActive_ &&
      ![[ComplexTextInputPanel sharedComplexTextInputPanel] inComposition]) {
    renderWidgetHostView_->PluginImeCompositionCompleted(
        base::string16(), focusedPluginIdentifier_);
    pluginImeActive_ = NO;
  }
}

// Overriding a NSResponder method to support application services.

- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  id requestor = nil;
  BOOL sendTypeIsString = [sendType isEqual:NSStringPboardType];
  BOOL returnTypeIsString = [returnType isEqual:NSStringPboardType];
  BOOL hasText = !renderWidgetHostView_->selected_text().empty();
  BOOL takesText =
      renderWidgetHostView_->text_input_type_ != ui::TEXT_INPUT_TYPE_NONE;

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

- (void)viewWillStartLiveResize {
  [super viewWillStartLiveResize];
  RenderWidgetHostImpl* widget = renderWidgetHostView_->render_widget_host_;
  if (widget)
    widget->Send(new ViewMsg_SetInLiveResize(widget->GetRoutingID(), true));
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];
  RenderWidgetHostImpl* widget = renderWidgetHostView_->render_widget_host_;
  if (widget)
    widget->Send(new ViewMsg_SetInLiveResize(widget->GetRoutingID(), false));
}

- (void)updateCursor:(NSCursor*)cursor {
  if (currentCursor_ == cursor)
    return;

  currentCursor_.reset([cursor retain]);
  [[self window] invalidateCursorRectsForView:self];
}

- (void)popupWindowWillClose:(NSNotification *)notification {
  renderWidgetHostView_->KillSelf();
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

  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithUTF8String:str.c_str()]);
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

- (BOOL)isOpaque {
  if (renderWidgetHostView_->use_core_animation_)
    return YES;
  return [super isOpaque];
}

// "-webkit-app-region: drag | no-drag" is implemented on Mac by excluding
// regions that are not draggable. (See ControlRegionView in
// native_app_window_cocoa.mm). This requires the render host view to be
// draggable by default.
- (BOOL)mouseDownCanMoveWindow {
  return YES;
}

@end

@implementation SoftwareLayer

- (id)initWithRenderWidgetHostViewMac:(content::RenderWidgetHostViewMac*)r {
  if (self = [super init]) {
    renderWidgetHostView_ = r;

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if (renderWidgetHostView_->software_frame_manager_->HasCurrentFrame() &&
        [self respondsToSelector:(@selector(setContentsScale:))]) {
      [self setContentsScale:renderWidgetHostView_->software_frame_manager_->
          GetCurrentFrameDeviceScaleFactor()];
    }

    // Ensure that the transition between frames not be animated.
    [self setActions:@{ @"contents" : [NSNull null] }];
  }
  return self;
}

- (void)drawInContext:(CGContextRef)context {
  TRACE_EVENT0("browser", "SoftwareLayer::drawInContext");

  CGRect clipRect = CGContextGetClipBoundingBox(context);
  if (renderWidgetHostView_) {
    [renderWidgetHostView_->cocoa_view() drawWithDirtyRect:clipRect
                                                 inContext:context];
  } else {
    CGContextSetFillColorWithColor(context,
                                   CGColorGetConstantColor(kCGColorWhite));
    CGContextFillRect(context, clipRect);
  }
}

- (void)disableRendering {
  // Disable the fade-out animation as the layer is removed.
  ScopedCAActionDisabler disabler;
  [self removeFromSuperlayer];
  renderWidgetHostView_ = nil;
}

@end  // implementation SoftwareLayer
