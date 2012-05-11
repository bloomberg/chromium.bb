// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/favicon_util.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/throbber_view.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/panels/panel_utils_cocoa.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"

using content::WebContents;

const int kMinimumWindowSize = 1;
const double kBoundsAnimationSpeedPixelsPerSecond = 1000;
const double kBoundsAnimationMaxDurationSeconds = 0.18;

// Resize edge thickness, in screen pixels.
const double kWidthOfMouseResizeArea = 4.0;
// The distance the user has to move the mouse while keeping the left button
// down before panel resizing operation actually starts.
const double kDragThreshold = 3.0;

// Replicate specific 10.6 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

enum {
  NSWindowCollectionBehaviorParticipatesInCycle = 1 << 5
};

#endif  // MAC_OS_X_VERSION_10_6

@interface PanelWindowControllerCocoa (PanelsCanBecomeKey)
// Internal helper method for extracting the total number of panel windows
// from the panel manager. Used to decide if panel can become the key window.
- (int)numPanels;
@end

@implementation PanelWindowCocoaImpl
// The panels cannot be reduced to 3-px windows on the edge of the screen
// active area (above Dock). Default constraining logic makes at least a height
// of the titlebar visible, so the user could still grab it. We do 'restore'
// differently, and minimize panels to 3 px. Hence the need to override the
// constraining logic.
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen {
  return frameRect;
}

// Prevent panel window from becoming key - for example when it is minimized.
// Panel windows use a higher priority NSWindowLevel to ensure they are always
// visible, causing the OS to prefer panel windows when selecting a window
// to make the key window. To counter this preference, we override
// -[NSWindow:canBecomeKeyWindow] to restrict when the panel can become the
// key window to a limited set of scenarios, such as when cycling through
// windows, when panels are the only remaining windows, when an event
// triggers window activation, etc. The panel may also be prevented from
// becoming the key window, regardless of the above scenarios, such as when
// a panel is minimized.
- (BOOL)canBecomeKeyWindow {
  // Give precedence to controller preventing activation of the window.
  PanelWindowControllerCocoa* controller =
      base::mac::ObjCCast<PanelWindowControllerCocoa>([self windowController]);
  if (![controller canBecomeKeyWindow])
    return NO;

  BrowserCrApplication* app = base::mac::ObjCCast<BrowserCrApplication>(
      [BrowserCrApplication sharedApplication]);

  // A Panel window can become the key window only in limited scenarios.
  // This prevents the system from always preferring a Panel window due
  // to its higher priority NSWindowLevel when selecting a window to make key.
  return ([app isHandlingSendEvent]  && [[app currentEvent] window] == self) ||
      [controller activationRequestedByBrowser] ||
      [app isCyclingWindows] ||
      [app previousKeyWindow] == self ||
      [[app windows] count] == static_cast<NSUInteger>([controller numPanels]);
}
@end

// Transparent view covering the whole panel in order to intercept mouse
// messages for custom user resizing. We need custom resizing because panels
// use their own constrained layout.
// TODO(dimich): Pull the start/stop drag logic into a separate base class and
// reuse between here and PanelTitlebarController.
@interface PanelResizeByMouseOverlay : NSView {
 @private
   Panel* panel_;
   NSPoint startMouseLocation_;
   PanelDragState dragState_;
   scoped_nsobject<NSCursor> dragCursor_;
   scoped_nsobject<NSCursor> eastWestCursor_;
   scoped_nsobject<NSCursor> northSouthCursor_;
   scoped_nsobject<NSCursor> northEastSouthWestCursor_;
   scoped_nsobject<NSCursor> northWestSouthEastCursor_;
   NSRect leftCursorRect_;
   NSRect rightCursorRect_;
   NSRect topCursorRect_;
   NSRect bottomCursorRect_;
   NSRect topLeftCursorRect_;
   NSRect topRightCursorRect_;
   NSRect bottomLeftCursorRect_;
   NSRect bottomRightCursorRect_;
}
@end

@implementation PanelResizeByMouseOverlay
- (PanelResizeByMouseOverlay*)initWithFrame:(NSRect)frame panel:(Panel*)panel {
  if ((self = [super initWithFrame:frame])) {
    panel_ = panel;
    // Initialize resize cursors, they are very likely to be needed so it's
    // better to pre-init them then stutter the mouse later when it hovers over
    // a resize edge. We use WebKit cursors that look similar to what OSX Lion
    // uses. NSCursor class does not yet have support for those new cursors.
    NSImage* image = gfx::GetCachedImageWithName(@"eastWestResizeCursor.png");
    DCHECK(image);
    eastWestCursor_.reset(
        [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(8,8)]);
    image = gfx::GetCachedImageWithName(@"northSouthResizeCursor.png");
    DCHECK(image);
    northSouthCursor_.reset(
        [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(8,8)]);
    image = gfx::GetCachedImageWithName(@"northEastSouthWestResizeCursor.png");
    DCHECK(image);
    northEastSouthWestCursor_.reset(
        [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(8,8)]);
    image = gfx::GetCachedImageWithName(@"northWestSouthEastResizeCursor.png");
    DCHECK(image);
    northWestSouthEastCursor_.reset(
        [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(8,8)]);
  }
  return self;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}

// |pointInWindow| is in window coordinates.
- (panel::ResizingSides)edgeHitTest:(NSPoint)pointInWindow {
  panel::Resizability resizability = panel_->CanResizeByMouse();
  DCHECK_NE(panel::NOT_RESIZABLE, resizability);

  NSPoint point = [self convertPoint:pointInWindow fromView:nil];
  BOOL flipped = [self isFlipped];
  if (NSMouseInRect(point, leftCursorRect_, flipped))
    return panel::RESIZE_LEFT;
  if (NSMouseInRect(point, rightCursorRect_, flipped))
    return panel::RESIZE_RIGHT;
  if (NSMouseInRect(point, topCursorRect_, flipped))
    return panel::RESIZE_TOP;
  if (NSMouseInRect(point, topLeftCursorRect_, flipped))
    return panel::RESIZE_TOP_LEFT;
  if (NSMouseInRect(point, topRightCursorRect_, flipped))
    return panel::RESIZE_TOP_RIGHT;

  // Bottom edge is not always resizable.
  if (panel::RESIZABLE_ALL_SIDES == resizability) {
    if (NSMouseInRect(point, bottomCursorRect_, flipped))
      return panel::RESIZE_BOTTOM;
    if (NSMouseInRect(point, bottomLeftCursorRect_, flipped))
      return panel::RESIZE_BOTTOM_LEFT;
    if (NSMouseInRect(point, bottomRightCursorRect_, flipped))
      return panel::RESIZE_BOTTOM_RIGHT;
  }

  return panel::RESIZE_NONE;
}

// NSWindow uses this method to figure out if this view is under the mouse
// and hence the one to handle the incoming mouse event.
// Since this view covers the whole panel, it is asked first.
// See if this is the mouse event we are interested in (in the resize areas)
// and return 'nil' to let NSWindow find another candidate otherwise.
// |point| is in coordinate system of the parent view.
- (NSView*)hitTest:(NSPoint)point {
  // If panel is not resizable, let the mouse events fall through.
  if (panel::NOT_RESIZABLE == panel_->CanResizeByMouse())
    return nil;

  NSPoint pointInWindow = [[self superview] convertPoint:point toView:nil];
  return [self edgeHitTest:pointInWindow] == panel::RESIZE_NONE ? nil : self;
}

- (void)mouseDown:(NSEvent*)event {
  // If the panel is not resizable, hitTest should have failed and no mouse
  // events should have came here.
  DCHECK_NE(panel::NOT_RESIZABLE, panel_->CanResizeByMouse());
  [self prepareForDrag:event];
}

- (void)mouseUp:(NSEvent*)event {
  // The mouseUp while in drag should be processed by nested message loop
  // in mouseDragged: method.
  DCHECK(dragState_ != PANEL_DRAG_IN_PROGRESS);
  // Cleanup in case the actual drag was not started (because of threshold).
  [self cleanupAfterDrag];
}

- (void)mouseDragged:(NSEvent*)event {
  if (dragState_ == PANEL_DRAG_SUPPRESSED)
    return;

  // In addition to events needed to control the drag operation, fetch the right
  // mouse click events and key down events and ignore them, to prevent their
  // accumulation in the queue and "playing out" when the mouse is released.
  const NSUInteger mask =
      NSLeftMouseUpMask | NSLeftMouseDraggedMask | NSKeyUpMask |
      NSRightMouseDownMask | NSKeyDownMask ;

  while (true) {
    base::mac::ScopedNSAutoreleasePool autorelease_pool;
    BOOL keepGoing = YES;

    switch ([event type]) {
      case NSLeftMouseDragged: {
        // Set the resize cursor on every mouse drag event in case the mouse
        // wandered outside the window and was switched to another one.
        // This does not produce flicker, seems the real cursor is updated after
        // mouseDrag is processed.
        [dragCursor_ set];

        // If drag didn't start yet, see if mouse moved far enough to start it.
        if (dragState_ == PANEL_DRAG_CAN_START && ![self tryStartDrag:event])
          return;

        DCHECK(dragState_ == PANEL_DRAG_IN_PROGRESS);
        [self resizeByMouse:[event locationInWindow]];
        break;
      }

      case NSKeyUp:
        if ([event keyCode] == kVK_Escape) {
          // The drag might not be started yet because of threshold, so check.
          if (dragState_ == PANEL_DRAG_IN_PROGRESS)
            [self endResize:YES];
          keepGoing = NO;
        }
        break;

      case NSLeftMouseUp:
        // The drag might not be started yet because of threshold, so check.
        if (dragState_ == PANEL_DRAG_IN_PROGRESS)
          [self endResize:NO];
        keepGoing = NO;
        break;

      case NSRightMouseDownMask:
        break;

      default:
        // Dequeue and ignore other mouse and key events so the Chrome context
        // menu does not come after right click on a page during Panel
        // resize, or the keystrokes are not 'accumulated' and entered
        // at once when the drag ends.
        break;
    }

    if (!keepGoing)
      break;

    autorelease_pool.Recycle();

    event = [NSApp nextEventMatchingMask:mask
                               untilDate:[NSDate distantFuture]
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:YES];

  }
  [self cleanupAfterDrag];
}

- (void)prepareForDrag:(NSEvent*)initialMouseDownEvent {
  dragState_ = PANEL_DRAG_CAN_START;
  startMouseLocation_ = [initialMouseDownEvent locationInWindow];

  // Make sure the cursor stays the same during whole resize operation.
  // The cursor rects normally do not guarantee the same cursor, since the
  // mouse may temporarily leave the cursor rect area (or even the window) so
  // the cursor will flicker. Disable cursor rects and grab the current cursor
  // so we can set it on mouseDragged: events to avoid flicker.
  [[self window] disableCursorRects];
  dragCursor_.reset([NSCursor currentCursor], scoped_policy::RETAIN);
}

-(void)cleanupAfterDrag {
  dragState_ = PANEL_DRAG_SUPPRESSED;
  [[self window] enableCursorRects];
  dragCursor_.reset();
  startMouseLocation_ = NSZeroPoint;
}

- (BOOL)exceedsDragThreshold:(NSPoint)mouseLocation {
  float deltaX = fabs(startMouseLocation_.x - mouseLocation.x);
  float deltaY = fabs(startMouseLocation_.y - mouseLocation.y);
  return deltaX > kDragThreshold || deltaY > kDragThreshold;
}

- (BOOL)tryStartDrag:(NSEvent*)event {
  DCHECK(dragState_ == PANEL_DRAG_CAN_START);
  NSPoint mouseLocation = [event locationInWindow];
  if (![self exceedsDragThreshold:mouseLocation])
    return NO;

  // Mouse moved over threshold, start drag.
  dragState_ = PANEL_DRAG_IN_PROGRESS;
  [self startResize:startMouseLocation_];
  return YES;
}

// |initialMouseLocation| is in window coordinates.
- (void)startResize:(NSPoint)initialMouseLocation {
  DCHECK(dragState_ == PANEL_DRAG_IN_PROGRESS);

  NSPoint initialMouseLocationScreen =
      [[self window] convertBaseToScreen:initialMouseLocation];

  panel_->manager()->StartResizingByMouse(
      panel_,
      cocoa_utils::ConvertPointFromCocoaCoordinates(initialMouseLocationScreen),
      [self edgeHitTest:initialMouseLocation]);
}

// |mouseLocation| is in window coordinates.
- (void)resizeByMouse:(NSPoint)mouseLocation {
  DCHECK(dragState_ == PANEL_DRAG_IN_PROGRESS);
  NSPoint mouseLocationScreen =
      [[self window] convertBaseToScreen:mouseLocation];
  panel_->manager()->ResizeByMouse(
      cocoa_utils::ConvertPointFromCocoaCoordinates(mouseLocationScreen));
}

- (void)endResize:(BOOL)cancelled {
  DCHECK(dragState_ == PANEL_DRAG_IN_PROGRESS);
  panel_->manager()->EndResizingByMouse(cancelled);
}

- (void)resetCursorRects {
  panel::Resizability resizability = panel_->CanResizeByMouse();
  if (panel::NOT_RESIZABLE == resizability)
    return;

  NSRect bounds = [self bounds];

  // Left vertical edge.
  leftCursorRect_ = NSMakeRect(NSMinX(bounds),
                               NSMinY(bounds) + kWidthOfMouseResizeArea,
                               kWidthOfMouseResizeArea,
                               NSHeight(bounds) - 2 * kWidthOfMouseResizeArea);
  [self addCursorRect:leftCursorRect_ cursor:eastWestCursor_];

  // Right vertical edge.
  rightCursorRect_ = leftCursorRect_;
  rightCursorRect_.origin.x = NSMaxX(bounds) - kWidthOfMouseResizeArea;
  [self addCursorRect:rightCursorRect_ cursor:eastWestCursor_];

  // Top horizontal edge.
  topCursorRect_ = NSMakeRect(NSMinX(bounds) + kWidthOfMouseResizeArea,
                              NSMaxY(bounds) - kWidthOfMouseResizeArea,
                              NSWidth(bounds) - 2 * kWidthOfMouseResizeArea,
                              kWidthOfMouseResizeArea);
  [self addCursorRect:topCursorRect_ cursor:northSouthCursor_];

  // Top left corner.
  topLeftCursorRect_ = NSMakeRect(NSMinX(bounds),
                                  NSMaxY(bounds) - kWidthOfMouseResizeArea,
                                  kWidthOfMouseResizeArea,
                                  NSMaxY(bounds));
  [self addCursorRect:topLeftCursorRect_ cursor:northWestSouthEastCursor_];

  // Top right corner.
  topRightCursorRect_ = topLeftCursorRect_;
  topRightCursorRect_.origin.x = NSMaxX(bounds) - kWidthOfMouseResizeArea;
  [self addCursorRect:topRightCursorRect_ cursor:northEastSouthWestCursor_];

  // Bottom edge is not always resizable.
  if (panel::RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM == resizability)
    return;

  // Bottom horizontal edge.
  bottomCursorRect_ = topCursorRect_;
  bottomCursorRect_.origin.y = NSMinY(bounds);
  [self addCursorRect:bottomCursorRect_ cursor:northSouthCursor_];

  // Bottom right corner.
  bottomRightCursorRect_ = topRightCursorRect_;
  bottomRightCursorRect_.origin.y = NSMinY(bounds);
  [self addCursorRect:bottomRightCursorRect_ cursor:northWestSouthEastCursor_];

  // Bottom left corner.
  bottomLeftCursorRect_ = bottomRightCursorRect_;
  bottomLeftCursorRect_.origin.x = NSMinX(bounds);
  [self addCursorRect:bottomLeftCursorRect_ cursor:northEastSouthWestCursor_];
}
@end

@implementation PanelWindowControllerCocoa

- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window {
  NSString* nibpath =
      [base::mac::FrameworkBundle() pathForResource:@"Panel" ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    windowShim_.reset(window);
    animateOnBoundsChange_ = YES;
    canBecomeKeyWindow_ = YES;
    contentsController_.reset(
        [[TabContentsController alloc] initWithContents:nil]);
  }
  return self;
}

- (ui::ThemeProvider*)themeProvider {
  return ThemeServiceFactory::GetForProfile(windowShim_->browser()->profile());
}

- (ThemedWindowStyle)themedWindowStyle {
  ThemedWindowStyle style = THEMED_POPUP;
  if (windowShim_->browser()->profile()->IsOffTheRecord())
    style |= THEMED_INCOGNITO;
  return style;
}

- (NSPoint)themePatternPhase {
  NSView* windowView = [[[self window] contentView] superview];
  return [BrowserWindowUtils themePatternPhaseFor:windowView withTabStrip:nil];
}

- (void)awakeFromNib {
  NSWindow* window = [self window];

  DCHECK(window);
  DCHECK(titlebar_view_);
  DCHECK_EQ(self, [window delegate]);

  [self updateWindowLevel];

  if (base::mac::IsOSSnowLeopardOrLater()) {
    [window setCollectionBehavior:
        NSWindowCollectionBehaviorParticipatesInCycle];
  }

  [titlebar_view_ attach];

  // Set initial size of the window to match the size of the panel to give
  // the renderer the proper size to work with earlier, avoiding a resize
  // after the window is revealed.
  gfx::Rect panelBounds = windowShim_->GetPanelBounds();
  NSRect frame = [window frame];
  frame.size.width = panelBounds.width();
  frame.size.height = panelBounds.height();
  [window setFrame:frame display:NO];

  [[window contentView] addSubview:[contentsController_ view]];
  [self enableTabContentsViewAutosizing];

  // Add a transparent overlay on top of the whole window to process mouse
  // events - for example, user-resizing.
  NSView* superview = [[window contentView] superview];
  NSRect bounds = [superview bounds];
  overlayView_.reset(
      [[PanelResizeByMouseOverlay alloc] initWithFrame:bounds
                                                 panel:windowShim_->panel()]);
    // Set autoresizing behavior: glued to edges.
  [overlayView_ setAutoresizingMask:(NSViewHeightSizable | NSViewWidthSizable)];
  [superview addSubview:overlayView_ positioned:NSWindowAbove relativeTo:nil];
}

- (void)disableTabContentsViewAutosizing {
  [[[self window] contentView] setAutoresizesSubviews:NO];
}

- (void)enableTabContentsViewAutosizing {
  NSView* contentView = [[self window] contentView];
  NSView* controllerView = [contentsController_ view];

  DCHECK([controllerView superview] == contentView);
  DCHECK([controllerView autoresizingMask] & NSViewHeightSizable);
  DCHECK([controllerView autoresizingMask] & NSViewWidthSizable);

  // Parent's bounds is child's frame.
  [controllerView setFrame:[contentView bounds]];
  [contentView setAutoresizesSubviews:YES];
  [contentsController_ ensureContentsVisible];
}

- (void)revealAnimatedWithFrame:(const NSRect&)frame {
  NSWindow* window = [self window];  // This ensures loading the nib.

  // Disable subview resizing while resizing the window to avoid renderer
  // resizes during intermediate stages of animation.
  [self disableTabContentsViewAutosizing];

  // We grow the window from the bottom up to produce a 'reveal' animation.
  NSRect startFrame = NSMakeRect(NSMinX(frame), NSMinY(frame),
                                 NSWidth(frame), kMinimumWindowSize);
  [window setFrame:startFrame display:NO animate:NO];
  // Shows the window without making it key, on top of its layer, even if
  // Chromium is not an active app.
  [window orderFrontRegardless];
  // TODO(dcheng): Temporary hack to work around the fact that
  // orderFrontRegardless causes us to become the first responder. The usual
  // Chrome assumption is that becoming the first responder = you have focus, so
  // we always deactivate the controls here. If we're created as an active
  // panel, we'll get a NSWindowDidBecomeKeyNotification and reactivate the web
  // view properly. See crbug.com/97831 for more details.
  WebContents* web_contents = [contentsController_ webContents];
  // RWHV may be NULL in unit tests.
  if (web_contents && web_contents->GetRenderWidgetHostView())
    web_contents->GetRenderWidgetHostView()->SetActive(false);

  // This will re-enable the content resizing after it finishes.
  [self setPanelFrame:frame animate:YES];
}

- (void)updateTitleBar {
  NSString* newTitle = base::SysUTF16ToNSString(
      windowShim_->browser()->GetWindowTitleForCurrentTab());
  pendingWindowTitle_.reset(
      [BrowserWindowUtils scheduleReplaceOldTitle:pendingWindowTitle_.get()
                                     withNewTitle:newTitle
                                        forWindow:[self window]]);
  [titlebar_view_ setTitle:newTitle];
  [self updateIcon];
}

- (void)updateIcon {
  NSView* icon = nil;
  NSRect iconFrame = [[titlebar_view_ icon] frame];
  if (throbberShouldSpin_) {
    // If the throbber is spinning now, no need to replace it.
    if ([[titlebar_view_ icon] isKindOfClass:[ThrobberView class]])
      return;

    NSImage* iconImage =
        ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_THROBBER);
    icon = [ThrobberView filmstripThrobberViewWithFrame:iconFrame
                                                  image:iconImage];
  } else {
    NSImage* iconImage = mac::FaviconForTabContents(
        windowShim_->browser()->GetSelectedTabContentsWrapper());
    if (!iconImage)
      iconImage = gfx::GetCachedImageWithName(@"nav.pdf");
    NSImageView* iconView =
        [[[NSImageView alloc] initWithFrame:iconFrame] autorelease];
    [iconView setImage:iconImage];
    icon = iconView;
  }

  [titlebar_view_ setIcon:icon];
}

- (void)updateThrobber:(BOOL)shouldSpin {
  if (throbberShouldSpin_ == shouldSpin)
    return;
  throbberShouldSpin_ = shouldSpin;

  // If the titlebar view has not been attached, bail out.
  if (!titlebar_view_)
    return;

  [self updateIcon];
}

- (void)updateTitleBarMinimizeRestoreButtonVisibility {
  Panel* panel = windowShim_->panel();
  [titlebar_view_ setMinimizeButtonVisibility:panel->CanMinimize()];
  [titlebar_view_ setRestoreButtonVisibility:panel->CanRestore()];
}

- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController {
  NSView* contentView = [[self window] contentView];
  [contentView addSubview:[findBarCocoaController view]];

  CGFloat maxY = NSMaxY([contentView frame]);
  CGFloat maxWidth = NSWidth([contentView frame]);
  [findBarCocoaController positionFindBarViewAtMaxY:maxY maxWidth:maxWidth];
}

- (void)tabInserted:(WebContents*)contents {
  [contentsController_ changeWebContents:contents];
  DCHECK(![[contentsController_ view] isHidden]);
}

- (void)tabDetached:(WebContents*)contents {
  DCHECK(contents == [contentsController_ webContents]);
  [contentsController_ changeWebContents:nil];
  [[contentsController_ view] setHidden:YES];
}

- (PanelTitlebarViewCocoa*)titlebarView {
  return titlebar_view_;
}

// Called to validate menu and toolbar items when this window is key. All the
// items we care about have been set with the |-commandDispatch:| or
// |-commandDispatchUsingKeyModifiers:| actions and a target of FirstResponder
// in IB. If it's not one of those, let it continue up the responder chain to be
// handled elsewhere.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:) ||
      action == @selector(commandDispatchUsingKeyModifiers:)) {
    NSInteger tag = [item tag];
    CommandUpdater* command_updater = windowShim_->browser()->command_updater();
    if (command_updater->SupportsCommand(tag)) {
      enable = command_updater->IsCommandEnabled(tag);
      // Disable commands that do not apply to Panels.
      switch (tag) {
        case IDC_CLOSE_TAB:
        case IDC_FULLSCREEN:
        case IDC_PRESENTATION_MODE:
        case IDC_SHOW_SYNC_SETUP:
          enable = NO;
          break;
        default:
          // Special handling for the contents of the Text Encoding submenu. On
          // Mac OS, instead of enabling/disabling the top-level menu item, we
          // enable/disable the submenu's contents (per Apple's HIG).
          EncodingMenuController encoding_controller;
          if (encoding_controller.DoesCommandBelongToEncodingMenu(tag)) {
            enable &= command_updater->IsCommandEnabled(IDC_ENCODING_MENU) ?
                YES : NO;
          }
      }
    }
  }
  return enable;
}

// Called when the user picks a menu or toolbar item when this window is key.
// Calls through to the browser object to execute the command. This assumes that
// the command is supported and doesn't check, otherwise it would have been
// disabled in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  DCHECK(sender);
  windowShim_->browser()->ExecuteCommand([sender tag]);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags.
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  DCHECK(sender);
  NSEvent* event = [NSApp currentEvent];
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEventWithFlags(
          event, [event modifierFlags]);
  windowShim_->browser()->ExecuteCommandWithDisposition(
      [sender tag], disposition);
}

- (void)executeCommand:(int)command {
  windowShim_->browser()->ExecuteCommandIfEnabled(command);
}

// Handler for the custom Close button.
- (void)closePanel {
  windowShim_->panel()->Close();
}

// Handler for the custom Minimize button.
- (void)minimizeButtonClicked:(int)modifierFlags {
  Panel* panel = windowShim_->panel();
  panel->OnMinimizeButtonClicked((modifierFlags & NSShiftKeyMask) ?
                                 panel::APPLY_TO_ALL : panel::NO_MODIFIER);
}

// Handler for the custom Restore button.
- (void)restoreButtonClicked:(int)modifierFlags {
  Panel* panel = windowShim_->panel();
  panel->OnRestoreButtonClicked((modifierFlags & NSShiftKeyMask) ?
                                panel::APPLY_TO_ALL : panel::NO_MODIFIER);
}

// Called when the user wants to close the panel or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onbeforeUnload handlers
// needing to be fired. If closing is deferred, the Browser will handle the
// processing required to get us to the closing state and (by watching for
// all the tabs going away) will again call to close the window when it's
// finally ready.
// This callback is only called if the standard Close button is enabled in XIB.
- (BOOL)windowShouldClose:(id)sender {
  Browser* browser = windowShim_->browser();
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser->ShouldCloseWindow())
    return NO;

  if (!browser->tabstrip_model()->empty()) {
    // Terminate any playing animations.
    [self terminateBoundsAnimation];
    animateOnBoundsChange_ = NO;
    // Tab strip isn't empty. Make browser to close all the tabs, allowing the
    // renderer to shut down and call us back again.
    // The tab strip of Panel is not visible and contains only one tab but
    // it still has to be closed.
    browser->OnWindowClosing();
    return NO;
  }

  // The tab strip is empty, it's ok to close the window.
  return YES;
}

// When windowShouldClose returns YES (or if controller receives direct 'close'
// signal), window will be unconditionally closed. Clean up.
- (void)windowWillClose:(NSNotification*)notification {
  DCHECK(windowShim_->browser()->tabstrip_model()->empty());
  // Avoid callbacks from a nonblocking animation in progress, if any.
  [self terminateBoundsAnimation];
  windowShim_->DidCloseNativeWindow();
  // Call |-autorelease| after a zero-length delay to avoid deadlock from
  // code in the current run loop that waits on BROWSER_CLOSED notification.
  // The notification is sent when this object is freed, but this object
  // cannot be freed until the current run loop completes.
  [self performSelector:@selector(autorelease)
             withObject:nil
             afterDelay:0];
}

- (void)startDrag:(NSPoint)mouseLocation {
  // Convert from Cocoa's screen coordinates to platform-indepedent screen
  // coordinates because PanelManager method takes platform-indepedent screen
  // coordinates.
  windowShim_->panel()->manager()->StartDragging(
      windowShim_->panel(),
      cocoa_utils::ConvertPointFromCocoaCoordinates(mouseLocation));
}

- (void)endDrag:(BOOL)cancelled {
  windowShim_->panel()->manager()->EndDragging(cancelled);
}

- (void)drag:(NSPoint)mouseLocation {
  // Convert from Cocoa's screen coordinates to platform-indepedent screen
  // coordinates because PanelManager method takes platform-indepedent screen
  // coordinates.
  windowShim_->panel()->manager()->Drag(
      cocoa_utils::ConvertPointFromCocoaCoordinates(mouseLocation));
}

- (void)setPanelFrame:(NSRect)frame
              animate:(BOOL)animate {
  BOOL jumpToDestination = (!animateOnBoundsChange_ || !animate);

  // If no animation is in progress, apply bounds change instantly.
  if (jumpToDestination && ![self isAnimatingBounds]) {
    [[self window] setFrame:frame display:YES animate:NO];
    return;
  }

  NSDictionary *windowResize = [NSDictionary dictionaryWithObjectsAndKeys:
      [self window], NSViewAnimationTargetKey,
      [NSValue valueWithRect:frame], NSViewAnimationEndFrameKey, nil];
  NSArray *animations = [NSArray arrayWithObjects:windowResize, nil];

  // If an animation is in progress, update the animation with new target
  // bounds. Also, set the destination frame bounds to the new value.
  if (jumpToDestination && [self isAnimatingBounds]) {
    [boundsAnimation_ setViewAnimations:animations];
    [[self window] setFrame:frame display:YES animate:NO];
    return;
  }

  // Will be enabled back in animationDidEnd callback.
  [self disableTabContentsViewAutosizing];

  // Terminate previous animation, if it is still playing.
  [self terminateBoundsAnimation];

  boundsAnimation_ =
      [[NSViewAnimation alloc] initWithViewAnimations:animations];
  [boundsAnimation_ setDelegate:self];

  NSRect currentFrame = [[self window] frame];
  // Compute duration. We use constant speed of animation, however if the change
  // is too large, we clip the duration (effectively increasing speed) to
  // limit total duration of animation. This makes 'small' transitions fast.
  // 'distance' is the max travel between 4 potentially traveling corners.
  double distanceX = std::max(abs(NSMinX(currentFrame) - NSMinX(frame)),
                              abs(NSMaxX(currentFrame) - NSMaxX(frame)));
  double distanceY = std::max(abs(NSMinY(currentFrame) - NSMinY(frame)),
                              abs(NSMaxY(currentFrame) - NSMaxY(frame)));
  double distance = std::max(distanceX, distanceY);
  double duration = std::min(distance / kBoundsAnimationSpeedPixelsPerSecond,
                             kBoundsAnimationMaxDurationSeconds);
  // Detect animation that happens when expansion state is set to MINIMIZED
  // and there is relatively big portion of the panel to hide from view.
  // Initialize animation differently in this case, using fast-pause-slow
  // method, see below for more details.
  if (distanceY > 0 &&
      windowShim_->panel()->expansion_state() == Panel::MINIMIZED) {
    animationStopToShowTitlebarOnly_ =
        1.0 - (windowShim_->TitleOnlyHeight() - NSHeight(frame)) / distanceY;
    if (animationStopToShowTitlebarOnly_ > 0.7) {  // Relatively big movement.
      playingMinimizeAnimation_ = YES;
      duration = 1.5;
    }
  }
  [boundsAnimation_ setDuration: PanelManager::AdjustTimeInterval(duration)];
  [boundsAnimation_ setFrameRate:0.0];
  [boundsAnimation_ setAnimationBlockingMode: NSAnimationNonblocking];
  [boundsAnimation_ startAnimation];
}

- (float)animation:(NSAnimation*)animation
  valueForProgress:(NSAnimationProgress)progress {
  return PanelBoundsAnimation::ComputeAnimationValue(
      progress, playingMinimizeAnimation_, animationStopToShowTitlebarOnly_);
}

- (void)cleanupAfterAnimation {
  playingMinimizeAnimation_ = NO;
  if (!windowShim_->panel()->IsMinimized())
    [self enableTabContentsViewAutosizing];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self cleanupAfterAnimation];

  // Only invoke this callback from animationDidEnd, since animationDidStop can
  // be called when we interrupt/restart animation which is in progress.
  // We only need this notification when animation indeed finished moving
  // the panel bounds.
  Panel* panel = windowShim_->panel();
  panel->manager()->OnPanelAnimationEnded(panel);
}

- (void)animationDidStop:(NSAnimation*)animation {
  [self cleanupAfterAnimation];
}

- (void)terminateBoundsAnimation {
  if (!boundsAnimation_)
    return;
  [boundsAnimation_ stopAnimation];
  [boundsAnimation_ setDelegate:nil];
  [boundsAnimation_ release];
  boundsAnimation_ = nil;
}

- (BOOL)isAnimatingBounds {
  return boundsAnimation_ && [boundsAnimation_ isAnimating];
}

- (void)onTitlebarMouseClicked:(int)modifierFlags {
  Panel* panel = windowShim_->panel();
  panel->OnTitlebarClicked((modifierFlags & NSShiftKeyMask) ?
                           panel::APPLY_TO_ALL : panel::NO_MODIFIER);
}

- (int)titlebarHeightInScreenCoordinates {
  NSView* titlebar = [self titlebarView];
  return NSHeight([titlebar convertRect:[titlebar bounds] toView:nil]);
}

- (void)ensureFullyVisible {
  // Shows the window without making it key, on top of its layer, even if
  // Chromium is not an active app.
  [[self window] orderFrontRegardless];
}

// TODO(dcheng): These two selectors are almost copy-and-paste from
// BrowserWindowController. Figure out the appropriate way of code sharing,
// whether it's refactoring more things into BrowserWindowUtils or making a
// common base controller for browser windows.
- (void)windowDidBecomeKey:(NSNotification*)notification {
  BrowserList::SetLastActive(windowShim_->browser());

  // We need to activate the controls (in the "WebView"). To do this, get the
  // selected WebContents's RenderWidgetHostView and tell it to activate.
  if (WebContents* contents = [contentsController_ webContents]) {
    if (content::RenderWidgetHostView* rwhv =
        contents->GetRenderWidgetHostView())
      rwhv->SetActive(true);
  }

  windowShim_->panel()->OnActiveStateChanged(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [self window]))
    return;

  // We need to deactivate the controls (in the "WebView"). To do this, get the
  // selected WebContents's RenderWidgetHostView and tell it to deactivate.
  if (WebContents* contents = [contentsController_ webContents]) {
    if (content::RenderWidgetHostView* rwhv =
        contents->GetRenderWidgetHostView())
      rwhv->SetActive(false);
  }

  windowShim_->panel()->OnActiveStateChanged(false);
}

- (void)deactivate {
  if (![[self window] isMainWindow])
    return;
  BrowserWindow* browser_window =
      windowShim_->panel()->manager()->GetNextBrowserWindowToActivate(
          windowShim_->panel());

  if (browser_window)
    browser_window->Activate();
  else
    [NSApp deactivate];
}

- (void)preventBecomingKeyWindow:(BOOL)prevent {
  canBecomeKeyWindow_ = !prevent;
}

- (void)fullScreenModeChanged:(bool)isFullScreen {
  [self updateWindowLevel];

  // The full-screen window is in normal level and changing the panel window to
  // same normal level will not move it below the full-screen window. Thus we
  // need to reorder the panel window.
  if (isFullScreen)
    [[self window] orderBack:nil];
  else
    [[self window] orderFrontRegardless];
}

- (BOOL)canBecomeKeyWindow {
  // Panel can only gain focus if it is expanded. Minimized panels do not
  // participate in Cmd-~ rotation.
  // TODO(dimich): If it will be ever desired to expand/focus the Panel on
  // keyboard navigation or via main menu, the care should be taken to avoid
  // cases when minimized Panel is getting keyboard input, invisibly.
  return canBecomeKeyWindow_;
}

- (int)numPanels {
  return windowShim_->panel()->manager()->num_panels();
}

- (BOOL)activationRequestedByBrowser {
  return windowShim_->ActivationRequestedByBrowser();
}

- (void)updateWindowLevel {
  if (![self isWindowLoaded])
    return;
  // Make sure we don't draw on top of a window in full screen mode.
  Panel* panel = windowShim_->panel();
  if (panel->manager()->display_settings_provider()->is_full_screen() ||
      !panel->always_on_top()) {
    [[self window] setLevel:NSNormalWindowLevel];
    return;
  }
  // If we simply use NSStatusWindowLevel (25) for all docked panel windows,
  // IME composition windows for things like CJK languages appear behind panels.
  // Pre 10.7, IME composition windows have a window level of 19, which is
  // lower than the dock at level 20. Since we want panels to appear on top of
  // the dock, it is impossible to enforce an ordering where IME > panel > dock,
  // since IME < dock.
  // On 10.7, IME composition windows and the dock both live at level 20, so we
  // use the same window level for panels. Since newly created windows appear at
  // the top of their window level, panels are typically on top of the dock, and
  // the IME composition window correctly draws over the panel.
  // An autohide dock causes problems though: since it's constantly being
  // revealed, it ends up drawing on top of other windows at the same level.
  // While this is OK for expanded panels, it makes minimized panels impossible
  // to activate. As a result, we still use NSStatusWindowLevel for minimized
  // panels, since it's impossible to compose IME text in them anyway.
  if (panel->IsMinimized()) {
    [[self window] setLevel:NSStatusWindowLevel];
    return;
  }
  [[self window] setLevel:NSDockWindowLevel];
}

- (void)enableResizeByMouse:(BOOL)enable {
  if (![self isWindowLoaded])
    return;
  [[self window] invalidateCursorRectsForView:overlayView_];
}

@end
