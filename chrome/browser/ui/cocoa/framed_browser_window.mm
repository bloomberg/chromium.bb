// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/framed_browser_window.h"

#include <math.h>
#include <objc/runtime.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"
#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/cocoa/nsgraphics_context_additions.h"
#import "ui/base/cocoa/nsview_additions.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

// Implementer's note: Moving the window controls is tricky. When altering the
// code, ensure that:
// - accessibility hit testing works
// - the accessibility hierarchy is correct
// - close/min in the background don't bring the window forward
// - rollover effects work correctly

// The NSLayoutConstraint class hierarchy only exists in the 10.11 SDK. When
// targeting something lower, constraintEqualToAnchor:constant: needs to be
// invoked using duck typing.
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11
@interface NSObject (NSLayoutConstraint)
- (NSLayoutConstraint*)constraintEqualToAnchor:(id)anchor constant:(CGFloat)c;
- (NSLayoutConstraint*)constraintEqualToAnchor:(id)anchor;
@end
#endif

namespace {

// Size of the gradient. Empirically determined so that the gradient looks
// like what the heuristic does when there are just a few tabs.
const CGFloat kWindowGradientHeight = 24.0;

}

@interface FramedBrowserWindow (Private)

// Updates the title bar's frame so it moves the windows buttons to correct
// location (frame bottom is moved down so the buttons are moved down as well).
- (void)adjustTitlebarContainer:(NSView*)titlebarContainer;
// Adds layout constraints to window buttons, respecting flag returned by
// |ShouldFlipWindowControlsInRTL| method.
- (void)setWindowButtonsConstraints;
// Replaces -[NSThemeFrame addTrackingArea:] with implementation that ignores
// tracking rect if its size is the same as the size of window buttons rect
// (rect where close, miniaturize and zoom buttons are located). This is
// needed to workaround macOS bug (rdar://28535344) which unnecessarily adds
// window buttons tracking rect even if those buttons were moved.
// TODO(crbug.com/651287): Remove this workaround once macOS bug is fixed.
- (void)forbidAddingWindowButtonsTrackingArea;
// Called when titlebar container changes its frame. This method adjusts
// titlebar container with correct frame.
- (void)titlebarDidChangeFrameNotification:(NSNotification*)notification;
// Adds layout constraints to the given window button so it displayed at correct
// location. This respects flag returned by |ShouldFlipWindowControlsInRTL|
// method.
- (void)setLeadingOffset:(CGFloat)leadingOffset
                toButton:(NSWindowButton)buttonType;

- (void)adjustCloseButton:(NSNotification*)notification;
- (void)adjustMiniaturizeButton:(NSNotification*)notification;
- (void)adjustZoomButton:(NSNotification*)notification;
- (void)adjustButton:(NSButton*)button
              ofKind:(NSWindowButton)kind;
- (void)childWindowsDidChange;

@end

@implementation FramedBrowserWindow

+ (CGFloat)browserFrameViewPaintHeight {
  return chrome::ShouldUseFullSizeContentView() ? chrome::kTabStripHeight
                                                : 60.0;
}

- (void)setStyleMask:(NSUInteger)styleMask {
  if (styleMaskLock_)
    return;
  [super setStyleMask:styleMask];
}

- (id)initWithContentRect:(NSRect)contentRect
              hasTabStrip:(BOOL)hasTabStrip{
  NSUInteger styleMask = NSTitledWindowMask |
                         NSClosableWindowMask |
                         NSMiniaturizableWindowMask |
                         NSResizableWindowMask |
                         NSTexturedBackgroundWindowMask;
  bool shouldUseFullSizeContentView =
      chrome::ShouldUseFullSizeContentView() && hasTabStrip;
  if (shouldUseFullSizeContentView) {
    styleMask |= NSFullSizeContentViewWindowMask;
  }

  if ((self = [super initWithContentRect:contentRect
                               styleMask:styleMask
                                 backing:NSBackingStoreBuffered
                                   defer:YES
                  wantsViewsOverTitlebar:hasTabStrip])) {
    // The 10.6 fullscreen code copies the title to a different window, which
    // will assert if it's nil.
    [self setTitle:@""];

    // The following two calls fix http://crbug.com/25684 by preventing the
    // window from recalculating the border thickness as the window is
    // resized.
    // This was causing the window tint to change for the default system theme
    // when the window was being resized.
    [self setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [self setContentBorderThickness:kWindowGradientHeight forEdge:NSMaxYEdge];

    hasTabStrip_ = hasTabStrip;
    closeButton_ = [self standardWindowButton:NSWindowCloseButton];
    miniaturizeButton_ = [self standardWindowButton:NSWindowMiniaturizeButton];
    zoomButton_ = [self standardWindowButton:NSWindowZoomButton];

    windowButtonsInterButtonSpacing_ =
        NSMinX([miniaturizeButton_ frame]) - NSMaxX([closeButton_ frame]);
    if (windowButtonsInterButtonSpacing_ < 0)
      // Sierra RTL
      windowButtonsInterButtonSpacing_ =
          NSMinX([miniaturizeButton_ frame]) - NSMaxX([zoomButton_ frame]);

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    if (shouldUseFullSizeContentView) {
      // If Chrome uses full sized content view then window buttons are placed
      // inside titlebar (which height is 22 points). In order to move window
      // buttons down the whole toolbar should be moved down.
      DCHECK(closeButton_);
      NSView* titlebarContainer = [[closeButton_ superview] superview];
      [self adjustTitlebarContainer:titlebarContainer];
      [center addObserver:self
                 selector:@selector(titlebarDidChangeFrameNotification:)
                     name:NSViewFrameDidChangeNotification
                   object:titlebarContainer];
      // Window buttons are not movable unless their positioning is forced via
      // layout constraints.
      [self setWindowButtonsConstraints];
      // Remove an extra tracking rect unnecessarily added by AppKit which
      // highlights the buttons on mouse enter event. That rect is added where
      // buttons used to be previously.
      [self forbidAddingWindowButtonsTrackingArea];
    } else {
      // If Chrome does not use a full sized content view then AppKit adds the
      // window buttons to the root view, where they must be manually
      // re-positioned.
      [self adjustButton:closeButton_ ofKind:NSWindowCloseButton];
      [self adjustButton:miniaturizeButton_ ofKind:NSWindowMiniaturizeButton];
      [self adjustButton:zoomButton_ ofKind:NSWindowZoomButton];
      [closeButton_ setPostsFrameChangedNotifications:YES];
      [miniaturizeButton_ setPostsFrameChangedNotifications:YES];
      [zoomButton_ setPostsFrameChangedNotifications:YES];

      [center addObserver:self
                 selector:@selector(adjustCloseButton:)
                     name:NSViewFrameDidChangeNotification
                   object:closeButton_];
      [center addObserver:self
                 selector:@selector(adjustMiniaturizeButton:)
                     name:NSViewFrameDidChangeNotification
                   object:miniaturizeButton_];
      [center addObserver:self
                 selector:@selector(adjustZoomButton:)
                     name:NSViewFrameDidChangeNotification
                   object:zoomButton_];
    }
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)adjustTitlebarContainer:(NSView*)titlebarContainer {
  DCHECK(chrome::ShouldUseFullSizeContentView());
  DCHECK([NSStringFromClass([titlebarContainer class])
      isEqual:@"NSTitlebarContainerView"]);

  NSRect newFrame = [titlebarContainer frame];
  NSRect superviewFrame = [[titlebarContainer superview] frame];
  // Increase toolbar height to move window buttons down where they should be.
  newFrame.size.height =
      floor((chrome::kTabStripHeight + NSHeight([closeButton_ frame])) / 2.0);
  newFrame.size.width = NSWidth(superviewFrame);
  newFrame.origin.y = NSHeight(superviewFrame) - NSHeight(newFrame);
  newFrame.origin.x = NSMinX(superviewFrame);
  [titlebarContainer setFrame:newFrame];
}

- (void)setWindowButtonsConstraints {
  DCHECK(chrome::ShouldUseFullSizeContentView());

  CGFloat leadingOffset =
      hasTabStrip_ ? kFramedWindowButtonsWithTabStripOffsetFromLeft
                   : kFramedWindowButtonsWithoutTabStripOffsetFromLeft;
  [self setLeadingOffset:leadingOffset toButton:NSWindowCloseButton];

  leadingOffset +=
      windowButtonsInterButtonSpacing_ + NSWidth([closeButton_ frame]);
  [self setLeadingOffset:leadingOffset toButton:NSWindowMiniaturizeButton];

  leadingOffset +=
      windowButtonsInterButtonSpacing_ + NSWidth([miniaturizeButton_ frame]);
  [self setLeadingOffset:leadingOffset toButton:NSWindowZoomButton];
}

- (void)forbidAddingWindowButtonsTrackingArea {
  DCHECK(chrome::ShouldUseFullSizeContentView());

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    NSView* themeFrame = [[[closeButton_ superview] superview] superview];
    Class themeFrameClass = [themeFrame class];
    DCHECK([NSStringFromClass(themeFrameClass) isEqual:@"NSThemeFrame"]);
    SEL addTrackingAreaSelector = @selector(addTrackingArea:);
    Method originalMethod =
        class_getInstanceMethod(themeFrameClass, addTrackingAreaSelector);
    IMP originalImp = method_getImplementation(originalMethod);
    NSRect windowButtonsRect = NSUnionRect(
        NSUnionRect([closeButton_ frame], [miniaturizeButton_ frame]),
        [zoomButton_ frame]);
    NSSize buttonsAreaSize = NSIntegralRect(windowButtonsRect).size;

    // |newImp| is never released with |imp_removeBlock|.
    IMP newImp = imp_implementationWithBlock(^(id self, id area) {
      // There is no other way to ensure that |area| is responsible for buttons
      // highlighting except by relying on its size.
      if (!NSEqualSizes(buttonsAreaSize, NSIntegralRect([area rect]).size)) {
        originalImp(self, addTrackingAreaSelector, area);
      }
    });

    // Do not use base::mac::ScopedObjCClassSwizzler as it replaces existing
    // implementation which is defined in NSView and will affect the whole app
    // performance.
    class_replaceMethod(themeFrameClass, addTrackingAreaSelector, newImp,
                        method_getTypeEncoding(originalMethod));
  });
}

- (void)titlebarDidChangeFrameNotification:(NSNotification*)notification {
  [self adjustTitlebarContainer:[notification object]];
}

- (void)setLeadingOffset:(CGFloat)leadingOffset
                toButton:(NSWindowButton)buttonType {
  DCHECK(chrome::ShouldUseFullSizeContentView());

  NSButton* button = [self standardWindowButton:buttonType];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  // Do not use leadingAnchor because |ShouldFlipWindowControlsInRTL|
  // should determine if current locale is RTL.
  NSLayoutXAxisAnchor* leadingSourceAnchor = [button leftAnchor];
  NSLayoutXAxisAnchor* leadingTargetAnchor = [[button superview] leftAnchor];
  if (cocoa_l10n_util::ShouldFlipWindowControlsInRTL()) {
    leadingSourceAnchor = [button rightAnchor];
    leadingTargetAnchor = [[button superview] rightAnchor];
    leadingOffset = -leadingOffset;
  }

#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11
  id leadingSourceAnchorDuck = leadingSourceAnchor;
#else
  NSLayoutXAxisAnchor* leadingSourceAnchorDuck = leadingSourceAnchor;
#endif
  [[leadingSourceAnchorDuck constraintEqualToAnchor:leadingTargetAnchor
                                           constant:leadingOffset]
      setActive:YES];

  [[[button bottomAnchor]
      constraintEqualToAnchor:[[button superview] bottomAnchor]]
          setActive:YES];
}

- (void)adjustCloseButton:(NSNotification*)notification {
  [self adjustButton:[notification object]
              ofKind:NSWindowCloseButton];
}

- (void)adjustMiniaturizeButton:(NSNotification*)notification {
  [self adjustButton:[notification object]
              ofKind:NSWindowMiniaturizeButton];
}

- (void)adjustZoomButton:(NSNotification*)notification {
  [self adjustButton:[notification object]
              ofKind:NSWindowZoomButton];
}

- (void)adjustButton:(NSButton*)button
              ofKind:(NSWindowButton)kind {
  NSRect buttonFrame = [button frame];

  CGFloat xOffset = hasTabStrip_
      ? kFramedWindowButtonsWithTabStripOffsetFromLeft
      : kFramedWindowButtonsWithoutTabStripOffsetFromLeft;
  CGFloat yOffset = hasTabStrip_
      ? kFramedWindowButtonsWithTabStripOffsetFromTop
      : kFramedWindowButtonsWithoutTabStripOffsetFromTop;
  buttonFrame.origin =
      NSMakePoint(xOffset, (NSHeight([self frame]) -
                            NSHeight(buttonFrame) - yOffset));

  switch (kind) {
    case NSWindowZoomButton:
      buttonFrame.origin.x += NSWidth([miniaturizeButton_ frame]);
      buttonFrame.origin.x += windowButtonsInterButtonSpacing_;
      // fallthrough
    case NSWindowMiniaturizeButton:
      buttonFrame.origin.x += NSWidth([closeButton_ frame]);
      buttonFrame.origin.x += windowButtonsInterButtonSpacing_;
      // fallthrough
    default:
      break;
  }

  if (cocoa_l10n_util::ShouldFlipWindowControlsInRTL()) {
    buttonFrame.origin.x =
        NSWidth([self frame]) - buttonFrame.origin.x - NSWidth([button frame]);
  }

  BOOL didPost = [button postsBoundsChangedNotifications];
  [button setPostsFrameChangedNotifications:NO];
  [button setFrame:buttonFrame];
  [button setPostsFrameChangedNotifications:didPost];
}

// The tab strip view covers our window buttons. So we add hit testing here
// to find them properly and return them to the accessibility system.
- (id)accessibilityHitTest:(NSPoint)point {
  NSPoint windowPoint = ui::ConvertPointFromScreenToWindow(self, point);
  NSControl* controls[] = { closeButton_, zoomButton_, miniaturizeButton_ };
  id value = nil;
  for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
    if (NSPointInRect(windowPoint, [controls[i] frame])) {
      value = [controls[i] accessibilityHitTest:point];
      break;
    }
  }
  if (!value) {
    value = [super accessibilityHitTest:point];
  }
  return value;
}

- (void)setShouldHideTitle:(BOOL)flag {
  shouldHideTitle_ = flag;
}

- (void)setStyleMaskLock:(BOOL)lock {
  styleMaskLock_ = lock;
}

- (BOOL)_isTitleHidden {
  return shouldHideTitle_;
}

- (CGFloat)windowButtonsInterButtonSpacing {
  return windowButtonsInterButtonSpacing_;
}

// This method is called whenever a window is moved in order to ensure it fits
// on the screen.  We cannot always handle resizes without breaking, so we
// prevent frame constraining in those cases.
- (NSRect)constrainFrameRect:(NSRect)frame toScreen:(NSScreen*)screen {
  // Do not constrain the frame rect if our delegate says no.  In this case,
  // return the original (unconstrained) frame.
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(shouldConstrainFrameRect)] &&
      ![delegate shouldConstrainFrameRect])
    return frame;

  return [super constrainFrameRect:frame toScreen:screen];
}

- (NSPoint)fullScreenButtonOriginAdjustment {
  if (!hasTabStrip_)
    return NSZeroPoint;

  // Vertically center the button.
  NSPoint origin = NSMakePoint(0, -6);

  // If there is a profile avatar icon present, shift the button over by its
  // width and some padding. The new avatar button is displayed to the right
  // of the fullscreen icon, so it doesn't need to be shifted.
  BrowserWindowController* bwc =
      base::mac::ObjCCastStrict<BrowserWindowController>(
          [self windowController]);
  if ([bwc shouldShowAvatar] && ![bwc shouldUseNewAvatarButton]) {
    NSView* avatarButton = [[bwc avatarButtonController] view];
    origin.x = -(NSWidth([avatarButton frame]) + 3);
  } else {
    origin.x -= 6;
  }

  return origin;
}

+ (BOOL)drawWindowThemeInDirtyRect:(NSRect)dirtyRect
                           forView:(NSView*)view
                            bounds:(NSRect)bounds
              forceBlackBackground:(BOOL)forceBlackBackground {
  const ui::ThemeProvider* themeProvider = [[view window] themeProvider];
  if (!themeProvider)
    return NO;

  ThemedWindowStyle windowStyle = [[view window] themedWindowStyle];

  // Devtools windows don't get themed.
  if (windowStyle & THEMED_DEVTOOLS)
    return NO;

  BOOL active = [[view window] isMainWindow];
  BOOL incognito = windowStyle & THEMED_INCOGNITO;
  BOOL popup = windowStyle & THEMED_POPUP;

  // Find a theme image.
  NSColor* themeImageColor = nil;
  if (!popup) {
    int themeImageID;
    if (active && incognito)
      themeImageID = IDR_THEME_FRAME_INCOGNITO;
    else if (active && !incognito)
      themeImageID = IDR_THEME_FRAME;
    else if (!active && incognito)
      themeImageID = IDR_THEME_FRAME_INCOGNITO_INACTIVE;
    else
      themeImageID = IDR_THEME_FRAME_INACTIVE;
    if (themeProvider->HasCustomImage(IDR_THEME_FRAME))
      themeImageColor = themeProvider->GetNSImageColorNamed(themeImageID);
  }

  BOOL themed = NO;
  if (themeImageColor) {
    // Default to replacing any existing pixels with the theme image, but if
    // asked paint black first and blend the theme with black.
    NSCompositingOperation operation = NSCompositeCopy;
    if (forceBlackBackground) {
      [[NSColor blackColor] set];
      NSRectFill(dirtyRect);
      operation = NSCompositeSourceOver;
    }

    NSPoint position = [[view window] themeImagePositionForAlignment:
        THEME_IMAGE_ALIGN_WITH_FRAME];
    [[NSGraphicsContext currentContext] cr_setPatternPhase:position
                                                   forView:view];

    [themeImageColor set];
    NSRectFillUsingOperation(dirtyRect, operation);
    themed = YES;
  }

  // Check to see if we have an overlay image.
  NSImage* overlayImage = nil;
  if (themeProvider->HasCustomImage(IDR_THEME_FRAME_OVERLAY) && !incognito &&
      !popup) {
    overlayImage = themeProvider->
        GetNSImageNamed(active ? IDR_THEME_FRAME_OVERLAY :
                                 IDR_THEME_FRAME_OVERLAY_INACTIVE);
  }

  if (overlayImage) {
    // Anchor to top-left and don't scale.
    NSPoint position = [[view window] themeImagePositionForAlignment:
        THEME_IMAGE_ALIGN_WITH_FRAME];
    position = [view convertPoint:position fromView:nil];
    NSSize overlaySize = [overlayImage size];
    NSRect imageFrame = NSMakeRect(0, 0, overlaySize.width, overlaySize.height);
    [overlayImage drawAtPoint:NSMakePoint(position.x,
                                          position.y - overlaySize.height)
                     fromRect:imageFrame
                    operation:NSCompositeSourceOver
                     fraction:1.0];
  }

  return themed;
}

- (NSTouchBar*)makeTouchBar {
  BrowserWindowController* bwc =
      base::mac::ObjCCastStrict<BrowserWindowController>(
          [self windowController]);
  return [[bwc browserWindowTouchBar] makeTouchBar];
}

- (NSColor*)titleColor {
  const ui::ThemeProvider* themeProvider = [self themeProvider];
  if (!themeProvider)
    return [NSColor windowFrameTextColor];

  ThemedWindowStyle windowStyle = [self themedWindowStyle];
  BOOL incognito = windowStyle & THEMED_INCOGNITO;

  if (incognito)
    return [NSColor whiteColor];
  else
    return [NSColor windowFrameTextColor];
}

- (void)addChildWindow:(NSWindow*)childWindow
               ordered:(NSWindowOrderingMode)orderingMode {
  [super addChildWindow:childWindow ordered:orderingMode];
  [self childWindowsDidChange];
}

- (void)removeChildWindow:(NSWindow*)childWindow {
  [super removeChildWindow:childWindow];
  [self childWindowsDidChange];
}

- (void)childWindowsDidChange {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(childWindowsDidChange)])
    [delegate childWindowsDidChange];
}

@end
