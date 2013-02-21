// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/native_app_window_cocoa.h"

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/ui/cocoa/extensions/extension_keybinding_registry_cocoa.h"
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "third_party/skia/include/core/SkRegion.h"

@interface NSWindow (NSPrivateApis)
- (void)setBottomCornerRounded:(BOOL)rounded;
@end

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSWindow (LionSDKDeclarations)
- (void)toggleFullScreen:(id)sender;
@end

#endif  // MAC_OS_X_VERSION_10_7

@implementation NativeAppWindowController

@synthesize appWindow = appWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowWillClose();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidBecomeKey();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidResignKey();
}

- (void)windowDidResize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidResize();
}

- (void)windowDidMove:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidMove();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidMiniaturize();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidDeminiaturize();
}

- (void)executeCommand:(int)command {
  // No-op, swallow the event.
}

- (BOOL)handledByExtensionCommand:(NSEvent*)event {
  if (appWindow_)
    return appWindow_->HandledByExtensionCommand(event);
  return NO;
}

@end

// This is really a method on NSGrayFrame, so it should only be called on the
// view passed into -[NSWindow drawCustomFrameRect:forView:].
@interface NSView (PrivateMethods)
- (CGFloat)roundedCornerRadius;
@end

@interface ShellNSWindow : ChromeEventProcessingWindow

- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view;

@end

@implementation ShellNSWindow

- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view {
  [[NSBezierPath bezierPathWithRect:rect] addClip];
  [[NSColor clearColor] set];
  NSRectFill(rect);

  // Set up our clip.
  CGFloat cornerRadius = 4.0;
  if ([view respondsToSelector:@selector(roundedCornerRadius)])
    cornerRadius = [view roundedCornerRadius];
  [[NSBezierPath bezierPathWithRoundedRect:[view bounds]
                                   xRadius:cornerRadius
                                   yRadius:cornerRadius] addClip];
  [[NSColor whiteColor] set];
  NSRectFill(rect);
}

@end

@interface ShellFramelessNSWindow : ShellNSWindow

@end

@implementation ShellFramelessNSWindow

+ (NSRect)frameRectForContentRect:(NSRect)contentRect
                        styleMask:(NSUInteger)mask {
  return contentRect;
}

+ (NSRect)contentRectForFrameRect:(NSRect)frameRect
                        styleMask:(NSUInteger)mask {
  return frameRect;
}

- (NSRect)frameRectForContentRect:(NSRect)contentRect {
  return contentRect;
}

- (NSRect)contentRectForFrameRect:(NSRect)frameRect {
  return frameRect;
}

@end

@interface ControlRegionView : NSView {
 @private
  NativeAppWindowCocoa* appWindow_;  // Weak; owns self.
}

@end

@implementation ControlRegionView

- (id)initWithAppWindow:(NativeAppWindowCocoa*)appWindow {
  if ((self = [super init]))
    appWindow_ = appWindow;
  return self;
}

- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (NSView*)hitTest:(NSPoint)aPoint {
  if (appWindow_->use_system_drag() ||
      !appWindow_->IsWithinDraggableRegion(aPoint)) {
    return nil;
  }
  return self;
}

- (void)mouseDown:(NSEvent*)event {
  appWindow_->HandleMouseEvent(event);
}

- (void)mouseDragged:(NSEvent*)event {
  appWindow_->HandleMouseEvent(event);
}

@end

@interface NSView (WebContentsView)
- (void)setMouseDownCanMoveWindow:(BOOL)can_move;
@end

NativeAppWindowCocoa::NativeAppWindowCocoa(
    ShellWindow* shell_window,
    const ShellWindow::CreateParams& params)
    : shell_window_(shell_window),
      has_frame_(params.frame == ShellWindow::FRAME_CHROME),
      attention_request_id_(0),
      use_system_drag_(true) {
  // Flip coordinates based on the primary screen.
  NSRect main_screen_rect = [[[NSScreen screens] objectAtIndex:0] frame];
  NSRect cocoa_bounds = NSMakeRect(params.bounds.x(),
      NSHeight(main_screen_rect) - params.bounds.y() - params.bounds.height(),
      params.bounds.width(), params.bounds.height());

  // If coordinates are < 0, center window on primary screen
  if (params.bounds.x() == INT_MIN) {
    cocoa_bounds.origin.x =
        floor((NSWidth(main_screen_rect) - NSWidth(cocoa_bounds)) / 2);
  }
  if (params.bounds.y() == INT_MIN) {
    cocoa_bounds.origin.y =
        floor((NSHeight(main_screen_rect) - NSHeight(cocoa_bounds)) / 2);
  }

  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask |
                          NSTexturedBackgroundWindowMask;
  scoped_nsobject<NSWindow> window;
  if (has_frame_) {
    window.reset([[ShellNSWindow alloc]
        initWithContentRect:cocoa_bounds
                  styleMask:style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
  } else {
    window.reset([[ShellFramelessNSWindow alloc]
        initWithContentRect:cocoa_bounds
                  styleMask:style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
  }
  [window setTitle:base::SysUTF8ToNSString(extension()->name())];
  min_size_ = params.minimum_size;
  if (min_size_.width() || min_size_.height()) {
    [window setContentMinSize:
        NSMakeSize(min_size_.width(), min_size_.height())];
  }
  max_size_ = params.maximum_size;
  if (max_size_.width() || max_size_.height()) {
    CGFloat max_width = max_size_.width() ? max_size_.width() : CGFLOAT_MAX;
    CGFloat max_height = max_size_.height() ? max_size_.height() : CGFLOAT_MAX;
    [window setContentMaxSize:NSMakeSize(max_width, max_height)];
  }

  if (base::mac::IsOSSnowLeopard() &&
      [window respondsToSelector:@selector(setBottomCornerRounded:)])
    [window setBottomCornerRounded:NO];

  window_controller_.reset(
      [[NativeAppWindowController alloc] initWithWindow:window.release()]);

  NSView* view = web_contents()->GetView()->GetNativeView();
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  // By default, the whole frameless window is not draggable.
  if (!has_frame_) {
    gfx::Rect window_bounds(
        0, 0, NSWidth(cocoa_bounds), NSHeight(cocoa_bounds));
    system_drag_exclude_areas_.push_back(window_bounds);
  }

  InstallView();

  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setAppWindow:this];

  extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryCocoa(
      shell_window_->profile(),
      window,
      extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY,
      shell_window));
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &web_contents()->GetController()));
}

void NativeAppWindowCocoa::InstallView() {
  NSView* view = web_contents()->GetView()->GetNativeView();
  if (has_frame_) {
    [view setFrame:[[window() contentView] bounds]];
    [[window() contentView] addSubview:view];
    if (!max_size_.IsEmpty() && min_size_ == max_size_) {
      [[window() standardWindowButton:NSWindowZoomButton] setEnabled:NO];
      [window() setShowsResizeIndicator:NO];
    }
  } else {
    // TODO(jeremya): find a cleaner way to send this information to the
    // WebContentsViewCocoa view.
    DCHECK([view
        respondsToSelector:@selector(setMouseDownCanMoveWindow:)]);
    [view setMouseDownCanMoveWindow:YES];

    NSView* frameView = [[window() contentView] superview];
    [view setFrame:[frameView bounds]];
    [frameView addSubview:view];

    [[window() standardWindowButton:NSWindowZoomButton] setHidden:YES];
    [[window() standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[window() standardWindowButton:NSWindowCloseButton] setHidden:YES];

    // Some third-party OS X utilities check the zoom button's enabled state to
    // determine whether to show custom UI on hover, so we disable it here to
    // prevent them from doing so in a frameless app window.
    [[window() standardWindowButton:NSWindowZoomButton] setEnabled:NO];

    InstallDraggableRegionViews();
  }
}

void NativeAppWindowCocoa::UninstallView() {
  NSView* view = web_contents()->GetView()->GetNativeView();
  [view removeFromSuperview];
}

bool NativeAppWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

bool NativeAppWindowCocoa::IsMaximized() const {
  return [window() isZoomed];
}

bool NativeAppWindowCocoa::IsMinimized() const {
  return [window() isMiniaturized];
}

bool NativeAppWindowCocoa::IsFullscreen() const {
  return is_fullscreen_;
}

void NativeAppWindowCocoa::SetFullscreen(bool fullscreen) {
  if (fullscreen == is_fullscreen_)
    return;
  is_fullscreen_ = fullscreen;

  if (base::mac::IsOSLionOrLater()) {
    [window() toggleFullScreen:nil];
    return;
  }

  DCHECK(base::mac::IsOSSnowLeopard());

  // Fade to black.
  const CGDisplayReservationInterval kFadeDurationSeconds = 0.6;
  bool did_fade_out = false;
  CGDisplayFadeReservationToken token;
  if (CGAcquireDisplayFadeReservation(kFadeDurationSeconds, &token) ==
      kCGErrorSuccess) {
    did_fade_out = true;
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendNormal,
        kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, /*synchronous=*/true);
  }

  // Since frameless windows insert the WebContentsView into the NSThemeFrame
  // ([[window contentView] superview]), and since that NSThemeFrame is
  // destroyed and recreated when we change the styleMask of the window, we
  // need to remove the view from the window when we change the style, and
  // add it back afterwards.
  UninstallView();
  if (fullscreen) {
    restored_bounds_ = [window() frame];
    [window() setStyleMask:NSBorderlessWindowMask];
    [window() setFrame:[window()
        frameRectForContentRect:[[window() screen] frame]]
               display:YES];
    base::mac::RequestFullScreen(base::mac::kFullScreenModeAutoHideAll);
  } else {
    base::mac::ReleaseFullScreen(base::mac::kFullScreenModeAutoHideAll);
    NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                            NSMiniaturizableWindowMask | NSResizableWindowMask |
                            NSTexturedBackgroundWindowMask;
    [window() setStyleMask:style_mask];
    [window() setFrame:restored_bounds_ display:YES];
  }
  InstallView();

  // Fade back in.
  if (did_fade_out) {
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendSolidColor,
        kCGDisplayBlendNormal, 0.0, 0.0, 0.0, /*synchronous=*/false);
    CGReleaseDisplayFadeReservation(token);
  }
}

bool NativeAppWindowCocoa::IsFullscreenOrPending() const {
  return is_fullscreen_;
}

gfx::NativeWindow NativeAppWindowCocoa::GetNativeWindow() {
  return window();
}

gfx::Rect NativeAppWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [window() frame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

gfx::Rect NativeAppWindowCocoa::GetBounds() const {
  return GetRestoredBounds();
}

void NativeAppWindowCocoa::Show() {
  [window_controller_ showWindow:nil];
  [window() makeKeyAndOrderFront:window_controller_];
}

void NativeAppWindowCocoa::ShowInactive() {
  [window() orderFront:window_controller_];
}

void NativeAppWindowCocoa::Hide() {
  [window() orderOut:window_controller_];
}

void NativeAppWindowCocoa::Close() {
  [window() performClose:nil];
}

void NativeAppWindowCocoa::Activate() {
  [BrowserWindowUtils activateWindowForController:window_controller_];
}

void NativeAppWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void NativeAppWindowCocoa::Maximize() {
  // Zoom toggles so only call if not already maximized.
  if (!IsMaximized())
    [window() zoom:window_controller_];
}

void NativeAppWindowCocoa::Minimize() {
  [window() miniaturize:window_controller_];
}

void NativeAppWindowCocoa::Restore() {
  if (IsMaximized())
    [window() zoom:window_controller_];  // Toggles zoom mode.
  else if (IsMinimized())
    [window() deminiaturize:window_controller_];
}

void NativeAppWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  // Enforce minimum/maximum bounds.
  gfx::Rect checked_bounds = bounds;

  NSSize min_size = [window() minSize];
  if (bounds.width() < min_size.width)
    checked_bounds.set_width(min_size.width);
  if (bounds.height() < min_size.height)
    checked_bounds.set_height(min_size.height);
  NSSize max_size = [window() maxSize];
  if (checked_bounds.width() > max_size.width)
    checked_bounds.set_width(max_size.width);
  if (checked_bounds.height() > max_size.height)
    checked_bounds.set_height(max_size.height);

  NSRect cocoa_bounds = NSMakeRect(checked_bounds.x(), 0,
                                   checked_bounds.width(),
                                   checked_bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  cocoa_bounds.origin.y = NSHeight([screen frame]) - checked_bounds.bottom();

  [window() setFrame:cocoa_bounds display:YES];
}

void NativeAppWindowCocoa::UpdateWindowIcon() {
  // TODO(junmin): implement.
}

void NativeAppWindowCocoa::UpdateWindowTitle() {
  string16 title = shell_window_->GetTitle();
  [window() setTitle:base::SysUTF16ToNSString(title)];
}

void NativeAppWindowCocoa::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (has_frame_)
    return;

  // To use system drag, the window has to be marked as draggable with
  // non-draggable areas being excluded via overlapping views.
  // 1) If no draggable area is provided, the window is not draggable at all.
  // 2) If only one draggable area is given, as this is the most common
  //    case, use the system drag. The non-draggable areas that are opposite of
  //    the draggable area are computed.
  // 3) Otherwise, use the custom drag. As such, we lose the capability to
  //    support some features like snapping into other space.

  // Determine how to perform the drag by counting the number of draggable
  // areas.
  const extensions::DraggableRegion* draggable_area = NULL;
  use_system_drag_ = true;
  for (std::vector<extensions::DraggableRegion>::const_iterator iter =
           regions.begin();
       iter != regions.end();
       ++iter) {
    if (iter->draggable) {
      // If more than one draggable area is found, use custom drag.
      if (draggable_area) {
        use_system_drag_ = false;
        break;
      }
      draggable_area = &(*iter);
    }
  }

  if (use_system_drag_)
    UpdateDraggableRegionsForSystemDrag(regions, draggable_area);
  else
    UpdateDraggableRegionsForCustomDrag(regions);

  InstallDraggableRegionViews();
}

void NativeAppWindowCocoa::UpdateDraggableRegionsForSystemDrag(
    const std::vector<extensions::DraggableRegion>& regions,
    const extensions::DraggableRegion* draggable_area) {
  NSView* web_view = web_contents()->GetView()->GetNativeView();
  NSInteger web_view_width = NSWidth([web_view bounds]);
  NSInteger web_view_height = NSHeight([web_view bounds]);

  system_drag_exclude_areas_.clear();

  // The whole window is not draggable if no draggable area is given.
  if (!draggable_area) {
    gfx::Rect window_bounds(0, 0, web_view_width, web_view_height);
    system_drag_exclude_areas_.push_back(window_bounds);
    return;
  }

  // Otherwise, there is only one draggable area. Compute non-draggable areas
  // that are the opposite of the given draggable area, combined with the
  // remaining provided non-draggable areas.

  // Copy all given non-draggable areas.
  for (std::vector<extensions::DraggableRegion>::const_iterator iter =
           regions.begin();
       iter != regions.end();
       ++iter) {
    if (!iter->draggable)
      system_drag_exclude_areas_.push_back(iter->bounds);
  }

  gfx::Rect draggable_bounds = draggable_area->bounds;
  gfx::Rect non_draggable_bounds;

  // Add the non-draggable area above the given draggable area.
  if (draggable_bounds.y() > 0) {
    non_draggable_bounds.SetRect(0,
                                 0,
                                 web_view_width,
                                 draggable_bounds.y() - 1);
    system_drag_exclude_areas_.push_back(non_draggable_bounds);
  }

  // Add the non-draggable area below the given draggable area.
  if (draggable_bounds.bottom() < web_view_height) {
    non_draggable_bounds.SetRect(0,
                                 draggable_bounds.bottom() + 1,
                                 web_view_width,
                                 web_view_height - draggable_bounds.bottom());
    system_drag_exclude_areas_.push_back(non_draggable_bounds);
  }

  // Add the non-draggable area to the left of the given draggable area.
  if (draggable_bounds.x() > 0) {
    non_draggable_bounds.SetRect(0,
                                 draggable_bounds.y(),
                                 draggable_bounds.x() - 1,
                                 draggable_bounds.height());
    system_drag_exclude_areas_.push_back(non_draggable_bounds);
  }

  // Add the non-draggable area to the right of the given draggable area.
  if (draggable_bounds.right() < web_view_width) {
    non_draggable_bounds.SetRect(draggable_bounds.right() + 1,
                                 draggable_bounds.y(),
                                 web_view_width - draggable_bounds.right(),
                                 draggable_bounds.height());
    system_drag_exclude_areas_.push_back(non_draggable_bounds);
  }
}

void NativeAppWindowCocoa::UpdateDraggableRegionsForCustomDrag(
    const std::vector<extensions::DraggableRegion>& regions) {
  // We still need one ControlRegionView to cover the whole window such that
  // mouse events could be captured.
  NSView* web_view = web_contents()->GetView()->GetNativeView();
  gfx::Rect window_bounds(
      0, 0, NSWidth([web_view bounds]), NSHeight([web_view bounds]));
  system_drag_exclude_areas_.clear();
  system_drag_exclude_areas_.push_back(window_bounds);

  // Aggregate the draggable areas and non-draggable areas such that hit test
  // could be performed easily.
  draggable_region_.reset(ShellWindow::RawDraggableRegionsToSkRegion(regions));
}

void NativeAppWindowCocoa::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser ||
      event.type == content::NativeWebKeyboardEvent::Char) {
    return;
  }
  [window() redispatchKeyEvent:event.os_event];
}

void NativeAppWindowCocoa::InstallDraggableRegionViews() {
  DCHECK(!has_frame_);

  // All ControlRegionViews should be added as children of the WebContentsView,
  // because WebContentsView will be removed and re-added when entering and
  // leaving fullscreen mode.
  NSView* webView = web_contents()->GetView()->GetNativeView();
  NSInteger webViewHeight = NSHeight([webView bounds]);

  // Remove all ControlRegionViews that are added last time.
  // Note that [webView subviews] returns the view's mutable internal array and
  // it should be copied to avoid mutating the original array while enumerating
  // it.
  scoped_nsobject<NSArray> subviews([[webView subviews] copy]);
  for (NSView* subview in subviews.get())
    if ([subview isKindOfClass:[ControlRegionView class]])
      [subview removeFromSuperview];

  // Create and add ControlRegionView for each region that needs to be excluded
  // from the dragging.
  for (std::vector<gfx::Rect>::const_iterator iter =
           system_drag_exclude_areas_.begin();
       iter != system_drag_exclude_areas_.end();
       ++iter) {
    scoped_nsobject<NSView> controlRegion(
        [[ControlRegionView alloc] initWithAppWindow:this]);
    [controlRegion setFrame:NSMakeRect(iter->x(),
                                       webViewHeight - iter->bottom(),
                                       iter->width(),
                                       iter->height())];
    [webView addSubview:controlRegion];
  }
}

void NativeAppWindowCocoa::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    web_contents()->Focus();
    return;
  }
  NOTREACHED();
}

void NativeAppWindowCocoa::FlashFrame(bool flash) {
  if (flash) {
    attention_request_id_ = [NSApp requestUserAttention:NSInformationalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

bool NativeAppWindowCocoa::IsAlwaysOnTop() const {
  return false;
}

gfx::Insets NativeAppWindowCocoa::GetFrameInsets() const {
  if (!has_frame_)
    return gfx::Insets();

  // Flip the coordinates based on the main screen.
  NSInteger screen_height =
      NSHeight([[[NSScreen screens] objectAtIndex:0] frame]);

  NSRect frame_nsrect = [window() frame];
  gfx::Rect frame_rect(NSRectToCGRect(frame_nsrect));
  frame_rect.set_y(screen_height - NSMaxY(frame_nsrect));

  NSRect content_nsrect = [window() contentRectForFrameRect:frame_nsrect];
  gfx::Rect content_rect(NSRectToCGRect(content_nsrect));
  content_rect.set_y(screen_height - NSMaxY(content_nsrect));

  return frame_rect.InsetsFrom(content_rect);
}

void NativeAppWindowCocoa::WindowWillClose() {
  [window_controller_ setAppWindow:NULL];
  shell_window_->OnNativeWindowChanged();
  shell_window_->OnNativeClose();
}

void NativeAppWindowCocoa::WindowDidBecomeKey() {
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetActive(true);
}

void NativeAppWindowCocoa::WindowDidResignKey() {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == window()))
    return;

  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetActive(false);
}

void NativeAppWindowCocoa::WindowDidResize() {
  shell_window_->OnNativeWindowChanged();
}

void NativeAppWindowCocoa::WindowDidMove() {
  shell_window_->OnNativeWindowChanged();
}

void NativeAppWindowCocoa::WindowDidMiniaturize() {
  shell_window_->OnNativeWindowChanged();
}

void NativeAppWindowCocoa::WindowDidDeminiaturize() {
  shell_window_->OnNativeWindowChanged();
}

bool NativeAppWindowCocoa::HandledByExtensionCommand(NSEvent* event) {
  return extension_keybinding_registry_->ProcessKeyEvent(
      content::NativeWebKeyboardEvent(event));
}

void NativeAppWindowCocoa::HandleMouseEvent(NSEvent* event) {
  if ([event type] == NSLeftMouseDown) {
    last_mouse_location_ =
        [window() convertBaseToScreen:[event locationInWindow]];
  } else if ([event type] == NSLeftMouseDragged) {
    NSPoint current_mouse_location =
        [window() convertBaseToScreen:[event locationInWindow]];
    NSPoint frame_origin = [window() frame].origin;
    frame_origin.x += current_mouse_location.x - last_mouse_location_.x;
    frame_origin.y += current_mouse_location.y - last_mouse_location_.y;
    [window() setFrameOrigin:frame_origin];
    last_mouse_location_ = current_mouse_location;
  }
}

bool NativeAppWindowCocoa::IsWithinDraggableRegion(NSPoint point) const {
  if (!draggable_region_)
    return false;
  NSView* webView = web_contents()->GetView()->GetNativeView();
  NSInteger webViewHeight = NSHeight([webView bounds]);
  // |draggable_region_| is stored in local platform-indepdent coordiate system
  // while |point| is in local Cocoa coordinate system. Do the conversion
  // to match these two.
  return draggable_region_->contains(point.x, webViewHeight - point.y);
}

NativeAppWindowCocoa::~NativeAppWindowCocoa() {
}

ShellNSWindow* NativeAppWindowCocoa::window() const {
  NSWindow* window = [window_controller_ window];
  CHECK(!window || [window isKindOfClass:[ShellNSWindow class]]);
  return static_cast<ShellNSWindow*>(window);
}

// static
NativeAppWindow* NativeAppWindow::Create(
    ShellWindow* shell_window,
    const ShellWindow::CreateParams& params) {
  return new NativeAppWindowCocoa(shell_window, params);
}
