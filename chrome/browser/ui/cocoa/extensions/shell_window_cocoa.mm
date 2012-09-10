// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/shell_window_cocoa.h"

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/ui/cocoa/extensions/extension_keybinding_registry_cocoa.h"
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

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

@implementation ShellWindowController

@synthesize shellWindow = shellWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowWillClose();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowDidBecomeKey();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowDidResignKey();
}

- (void)windowDidResize:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowDidResize();
}

- (void)windowDidMove:(NSNotification*)notification {
  if (shellWindow_)
    shellWindow_->WindowDidMove();
}

- (void)gtm_systemRequestsVisibilityForView:(NSView*)view {
  [[self window] makeKeyAndOrderFront:self];
}

- (GTMWindowSheetController*)sheetController {
  if (!sheetController_.get()) {
    sheetController_.reset([[GTMWindowSheetController alloc]
        initWithWindow:[self window]
              delegate:self]);
  }
  return sheetController_;
}

- (void)executeCommand:(int)command {
  // No-op, swallow the event.
}

- (BOOL)handledByExtensionCommand:(NSEvent*)event {
  if (shellWindow_)
    return shellWindow_->HandledByExtensionCommand(event);
  return NO;
}

@end

@interface ShellNSWindow : ChromeEventProcessingWindow

- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view;

@end

// This is really a method on NSGrayFrame, so it should only be called on the
// view passed into -[NSWindow drawCustomFrameRect:forView:].
@interface NSView (PrivateMethods)
- (CGFloat)roundedCornerRadius;
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

@interface ControlRegionView : NSView
@end
@implementation ControlRegionView
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}
- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}
@end

@interface NSView (WebContentsView)
- (void)setMouseDownCanMoveWindow:(BOOL)can_move;
@end

ShellWindowCocoa::ShellWindowCocoa(ShellWindow* shell_window,
                                   const ShellWindow::CreateParams& params)
    : shell_window_(shell_window),
      has_frame_(params.frame == ShellWindow::CreateParams::FRAME_CHROME),
      attention_request_id_(0) {
  // Flip coordinates based on the primary screen.
  NSRect main_screen_rect = [[[NSScreen screens] objectAtIndex:0] frame];
  NSRect cocoa_bounds = NSMakeRect(params.bounds.x(),
      NSHeight(main_screen_rect) - params.bounds.y() - params.bounds.height(),
      params.bounds.width(), params.bounds.height());

  // If coordinates are < 0, center window on primary screen
  if (params.bounds.x() < 0) {
    cocoa_bounds.origin.x =
        (NSWidth(main_screen_rect) - NSWidth(cocoa_bounds)) / 2;
  }
  if (params.bounds.y() < 0) {
    cocoa_bounds.origin.y =
        (NSHeight(main_screen_rect) - NSHeight(cocoa_bounds)) / 2;
  }

  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask |
                          NSTexturedBackgroundWindowMask;
  scoped_nsobject<NSWindow> window([[ShellNSWindow alloc]
      initWithContentRect:cocoa_bounds
                styleMask:style_mask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [window setTitle:base::SysUTF8ToNSString(extension()->name())];
  gfx::Size min_size = params.minimum_size;
  if (min_size.width() || min_size.height()) {
    [window setContentMinSize:NSMakeSize(min_size.width(), min_size.height())];
  }
  gfx::Size max_size = params.maximum_size;
  if (max_size.width() || max_size.height()) {
    CGFloat max_width = max_size.width() ? max_size.width() : CGFLOAT_MAX;
    CGFloat max_height = max_size.height() ? max_size.height() : CGFLOAT_MAX;
    [window setContentMaxSize:NSMakeSize(max_width, max_height)];
  }

  if (base::mac::IsOSSnowLeopard() &&
      [window respondsToSelector:@selector(setBottomCornerRounded:)])
    [window setBottomCornerRounded:NO];

  window_controller_.reset(
      [[ShellWindowController alloc] initWithWindow:window.release()]);

  NSView* view = web_contents()->GetView()->GetNativeView();
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  InstallView();

  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setShellWindow:this];

  extension_keybinding_registry_.reset(
      new ExtensionKeybindingRegistryCocoa(shell_window_->profile(), window,
          extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY));
}

void ShellWindowCocoa::InstallView() {
  NSView* view = web_contents()->GetView()->GetNativeView();
  if (has_frame_) {
    [view setFrame:[[window() contentView] bounds]];
    [[window() contentView] addSubview:view];
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

    InstallDraggableRegionViews();
  }
}

void ShellWindowCocoa::UninstallView() {
  NSView* view = web_contents()->GetView()->GetNativeView();
  [view removeFromSuperview];
}

bool ShellWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

bool ShellWindowCocoa::IsMaximized() const {
  return [window() isZoomed];
}

bool ShellWindowCocoa::IsMinimized() const {
  return [window() isMiniaturized];
}

bool ShellWindowCocoa::IsFullscreen() const {
  return is_fullscreen_;
}

void ShellWindowCocoa::SetFullscreen(bool fullscreen) {
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

bool ShellWindowCocoa::IsFullscreenOrPending() const {
  return is_fullscreen_;
}

gfx::NativeWindow ShellWindowCocoa::GetNativeWindow() {
  return window();
}

gfx::Rect ShellWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [window() frame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

gfx::Rect ShellWindowCocoa::GetBounds() const {
  return GetRestoredBounds();
}

void ShellWindowCocoa::Show() {
  [window_controller_ showWindow:nil];
  [window() makeKeyAndOrderFront:window_controller_];
}

void ShellWindowCocoa::ShowInactive() {
  [window() orderFront:window_controller_];
}

void ShellWindowCocoa::Close() {
  [window() performClose:nil];
}

void ShellWindowCocoa::Activate() {
  [BrowserWindowUtils activateWindowForController:window_controller_];
}

void ShellWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void ShellWindowCocoa::Maximize() {
  // Zoom toggles so only call if not already maximized.
  if (!IsMaximized())
    [window() zoom:window_controller_];
}

void ShellWindowCocoa::Minimize() {
  [window() miniaturize:window_controller_];
}

void ShellWindowCocoa::Restore() {
  if (IsMaximized())
    [window() zoom:window_controller_];  // Toggles zoom mode.
  else if (IsMinimized())
    [window() deminiaturize:window_controller_];
}

void ShellWindowCocoa::SetBounds(const gfx::Rect& bounds) {
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

void ShellWindowCocoa::UpdateWindowIcon() {
  // TODO(junmin): implement.
}

void ShellWindowCocoa::UpdateWindowTitle() {
  string16 title = shell_window_->GetTitle();
  [window() setTitle:base::SysUTF16ToNSString(title)];
}

void ShellWindowCocoa::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (has_frame_)
    return;

  draggable_regions_ = regions;
  InstallDraggableRegionViews();
}

void ShellWindowCocoa::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser ||
      event.type == content::NativeWebKeyboardEvent::Char) {
    return;
  }
  [window() redispatchKeyEvent:event.os_event];
}

void ShellWindowCocoa::InstallDraggableRegionViews() {
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
  for (std::vector<extensions::DraggableRegion>::const_iterator iter =
           draggable_regions_.begin();
       iter != draggable_regions_.end();
       ++iter) {
    const extensions::DraggableRegion& region = *iter;
    scoped_nsobject<NSView> controlRegion([[ControlRegionView alloc] init]);
    [controlRegion setFrame:NSMakeRect(region.bounds.x(),
                                       webViewHeight - region.bounds.bottom(),
                                       region.bounds.width(),
                                       region.bounds.height())];
    [webView addSubview:controlRegion];
  }
}

void ShellWindowCocoa::FlashFrame(bool flash) {
  if (flash) {
    attention_request_id_ = [NSApp requestUserAttention:NSInformationalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

bool ShellWindowCocoa::IsAlwaysOnTop() const {
  return false;
}

void ShellWindowCocoa::WindowWillClose() {
  [window_controller_ setShellWindow:NULL];
  shell_window_->SaveWindowPosition();
  shell_window_->OnNativeClose();
}

void ShellWindowCocoa::WindowDidBecomeKey() {
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetActive(true);
}

void ShellWindowCocoa::WindowDidResignKey() {
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

void ShellWindowCocoa::WindowDidResize() {
  shell_window_->SaveWindowPosition();
}

void ShellWindowCocoa::WindowDidMove() {
  shell_window_->SaveWindowPosition();
}

bool ShellWindowCocoa::HandledByExtensionCommand(NSEvent* event) {
  return extension_keybinding_registry_->ProcessKeyEvent(
      content::NativeWebKeyboardEvent(event));
}

ShellWindowCocoa::~ShellWindowCocoa() {
}

ShellNSWindow* ShellWindowCocoa::window() const {
  NSWindow* window = [window_controller_ window];
  CHECK(!window || [window isKindOfClass:[ShellNSWindow class]]);
  return static_cast<ShellNSWindow*>(window);
}

// static
NativeShellWindow* NativeShellWindow::Create(
    ShellWindow* shell_window, const ShellWindow::CreateParams& params) {
  return new ShellWindowCocoa(shell_window, params);
}
