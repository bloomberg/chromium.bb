// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/framed_browser_window.h"

#include "base/logging.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/custom_frame_view.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "grit/theme_resources.h"
#include "ui/base/cocoa/nsgraphics_context_additions.h"

// Implementer's note: Moving the window controls is tricky. When altering the
// code, ensure that:
// - accessibility hit testing works
// - the accessibility hierarchy is correct
// - close/min in the background don't bring the window forward
// - rollover effects work correctly

namespace {

const CGFloat kBrowserFrameViewPaintHeight = 60.0;

// Size of the gradient. Empirically determined so that the gradient looks
// like what the heuristic does when there are just a few tabs.
const CGFloat kWindowGradientHeight = 24.0;

}

@interface FramedBrowserWindow (Private)

- (void)adjustCloseButton:(NSNotification*)notification;
- (void)adjustMiniaturizeButton:(NSNotification*)notification;
- (void)adjustZoomButton:(NSNotification*)notification;
- (void)adjustButton:(NSButton*)button
              ofKind:(NSWindowButton)kind;
- (NSView*)frameView;

@end

// Undocumented APIs. They are really on NSGrayFrame rather than NSView. Take
// care to only call them on the NSView passed into
// -[NSWindow drawCustomRect:forView:].
@interface NSView (UndocumentedAPI)

- (float)roundedCornerRadius;
- (CGRect)_titlebarTitleRect;
- (void)_drawTitleStringIn:(struct CGRect)arg1 withColor:(id)color;

@end


@implementation FramedBrowserWindow

- (id)initWithContentRect:(NSRect)contentRect
              hasTabStrip:(BOOL)hasTabStrip{
  NSUInteger styleMask = NSTitledWindowMask |
                         NSClosableWindowMask |
                         NSMiniaturizableWindowMask |
                         NSResizableWindowMask |
                         NSTexturedBackgroundWindowMask;
  if ((self = [super initWithContentRect:contentRect
                               styleMask:styleMask
                                 backing:NSBackingStoreBuffered
                                   defer:YES])) {
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
    [closeButton_ setPostsFrameChangedNotifications:YES];
    miniaturizeButton_ = [self standardWindowButton:NSWindowMiniaturizeButton];
    [miniaturizeButton_ setPostsFrameChangedNotifications:YES];
    zoomButton_ = [self standardWindowButton:NSWindowZoomButton];
    [zoomButton_ setPostsFrameChangedNotifications:YES];

    windowButtonsInterButtonSpacing_ =
        NSMinX([miniaturizeButton_ frame]) - NSMaxX([closeButton_ frame]);

    [self adjustButton:closeButton_ ofKind:NSWindowCloseButton];
    [self adjustButton:miniaturizeButton_ ofKind:NSWindowMiniaturizeButton];
    [self adjustButton:zoomButton_ ofKind:NSWindowZoomButton];

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
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
    [center addObserver:self
               selector:@selector(themeDidChangeNotification:)
                   name:kBrowserThemeDidChangeNotification
                 object:nil];
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
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
  NSRect frameViewBounds = [[self frameView] bounds];

  CGFloat xOffset = hasTabStrip_
      ? kFramedWindowButtonsWithTabStripOffsetFromLeft
      : kFramedWindowButtonsWithoutTabStripOffsetFromLeft;
  CGFloat yOffset = hasTabStrip_
      ? kFramedWindowButtonsWithTabStripOffsetFromTop
      : kFramedWindowButtonsWithoutTabStripOffsetFromTop;
  buttonFrame.origin =
      NSMakePoint(xOffset, (NSHeight(frameViewBounds) -
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

  BOOL didPost = [button postsBoundsChangedNotifications];
  [button setPostsFrameChangedNotifications:NO];
  [button setFrame:buttonFrame];
  [button setPostsFrameChangedNotifications:didPost];
}

- (NSView*)frameView {
  return [[self contentView] superview];
}

// The tab strip view covers our window buttons. So we add hit testing here
// to find them properly and return them to the accessibility system.
- (id)accessibilityHitTest:(NSPoint)point {
  NSPoint windowPoint = [self convertScreenToBase:point];
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

- (void)windowMainStatusChanged {
  NSView* frameView = [self frameView];
  NSView* contentView = [self contentView];
  NSRect updateRect = [frameView frame];
  NSRect contentRect = [contentView frame];
  CGFloat tabStripHeight = [TabStripController defaultTabHeight];
  updateRect.size.height -= NSHeight(contentRect) - tabStripHeight;
  updateRect.origin.y = NSMaxY(contentRect) - tabStripHeight;
  [[self frameView] setNeedsDisplayInRect:updateRect];
}

- (void)becomeMainWindow {
  [self windowMainStatusChanged];
  [super becomeMainWindow];
}

- (void)resignMainWindow {
  [self windowMainStatusChanged];
  [super resignMainWindow];
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  [[self frameView] setNeedsDisplay:YES];
}

- (void)sendEvent:(NSEvent*)event {
  // For Cocoa windows, clicking on the close and the miniaturize buttons (but
  // not the zoom button) while a window is in the background does NOT bring
  // that window to the front. We don't get that behavior for free (probably
  // because the tab strip view covers those buttons), so we handle it here.
  // Zoom buttons do bring the window to the front. Note that Finder windows (in
  // Leopard) behave differently in this regard in that zoom buttons don't bring
  // the window to the foreground.
  BOOL eventHandled = NO;
  if (![self isMainWindow]) {
    if ([event type] == NSLeftMouseDown) {
      NSView* frameView = [self frameView];
      NSPoint mouse = [frameView convertPoint:[event locationInWindow]
                                     fromView:nil];
      if (NSPointInRect(mouse, [closeButton_ frame])) {
        [closeButton_ mouseDown:event];
        eventHandled = YES;
      } else if (NSPointInRect(mouse, [miniaturizeButton_ frame])) {
        [miniaturizeButton_ mouseDown:event];
        eventHandled = YES;
      }
    }
  }
  if (!eventHandled) {
    [super sendEvent:event];
  }
}

- (void)setShouldHideTitle:(BOOL)flag {
  shouldHideTitle_ = flag;
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

// This method is overridden in order to send the toggle fullscreen message
// through the cross-platform browser framework before going fullscreen.  The
// message will eventually come back as a call to |-toggleSystemFullScreen|,
// which in turn calls AppKit's |NSWindow -toggleFullScreen:|.
- (void)toggleFullScreen:(id)sender {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(handleLionToggleFullscreen)])
    [delegate handleLionToggleFullscreen];
}

- (void)toggleSystemFullScreen {
  if ([super respondsToSelector:@selector(toggleFullScreen:)])
    [super toggleFullScreen:nil];
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
      static_cast<BrowserWindowController*>([self windowController]);
  if ([bwc shouldShowAvatar] && ![bwc shouldUseNewAvatarButton]) {
    NSView* avatarButton = [[bwc avatarButtonController] view];
    origin.x = -(NSWidth([avatarButton frame]) + 3);
  } else {
    origin.x -= 6;
  }

  return origin;
}

- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view {
  // WARNING: There is an obvious optimization opportunity here that you DO NOT
  // want to take. To save painting cycles, you might think it would be a good
  // idea to call out to the default implementation only if no theme were
  // drawn. In reality, however, if you fail to call the default
  // implementation, or if you call it after a clipping path is set, the
  // rounded corners at the top of the window will not draw properly. Do not
  // try to be smart here.

  // Only paint the top of the window.
  NSRect windowRect = [view convertRect:[self frame] fromView:nil];
  windowRect.origin = NSZeroPoint;

  NSRect paintRect = windowRect;
  paintRect.origin.y = NSMaxY(paintRect) - kBrowserFrameViewPaintHeight;
  paintRect.size.height = kBrowserFrameViewPaintHeight;
  rect = NSIntersectionRect(paintRect, rect);
  [super drawCustomFrameRect:rect forView:view];

  // Set up our clip.
  float cornerRadius = 4.0;
  if ([view respondsToSelector:@selector(roundedCornerRadius)])
    cornerRadius = [view roundedCornerRadius];
  [[NSBezierPath bezierPathWithRoundedRect:windowRect
                                   xRadius:cornerRadius
                                   yRadius:cornerRadius] addClip];
  [[NSBezierPath bezierPathWithRect:rect] addClip];

  // Do the theming.
  BOOL themed = [FramedBrowserWindow
      drawWindowThemeInDirtyRect:rect
                         forView:view
                          bounds:windowRect
            forceBlackBackground:NO];

  // If the window needs a title and we painted over the title as drawn by the
  // default window paint, paint it ourselves.
  if (themed && [view respondsToSelector:@selector(_titlebarTitleRect)] &&
      [view respondsToSelector:@selector(_drawTitleStringIn:withColor:)] &&
      ![self _isTitleHidden]) {
    [view _drawTitleStringIn:[view _titlebarTitleRect]
                   withColor:[self titleColor]];
  }

  // Pinstripe the top.
  if (themed) {
    CGFloat lineWidth = [view cr_lineWidth];

    windowRect = [view convertRect:[self frame] fromView:nil];
    windowRect.origin = NSZeroPoint;
    windowRect.origin.y -= 0.5 * lineWidth;
    windowRect.origin.x -= 0.5 * lineWidth;
    windowRect.size.width += lineWidth;
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.5] set];
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:windowRect
                                                         xRadius:cornerRadius
                                                         yRadius:cornerRadius];
    [path setLineWidth:lineWidth];
    [path stroke];
  }
}

+ (BOOL)drawWindowThemeInDirtyRect:(NSRect)dirtyRect
                           forView:(NSView*)view
                            bounds:(NSRect)bounds
              forceBlackBackground:(BOOL)forceBlackBackground {
  ui::ThemeProvider* themeProvider = [[view window] themeProvider];
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

  // If no theme image, use a gradient if incognito.
  NSGradient* gradient = nil;
  if (!themeImageColor && incognito)
    gradient = themeProvider->GetNSGradient(
        active ? ThemeProperties::GRADIENT_FRAME_INCOGNITO :
                 ThemeProperties::GRADIENT_FRAME_INCOGNITO_INACTIVE);

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

    // Align the phase to physical pixels so resizing the window under HiDPI
    // doesn't cause wiggling of the theme.
    NSView* frameView = [[[view window] contentView] superview];
    position = [frameView convertPointToBase:position];
    position.x = floor(position.x);
    position.y = floor(position.y);
    position = [frameView convertPointFromBase:position];
    [[NSGraphicsContext currentContext] cr_setPatternPhase:position
                                                   forView:view];

    [themeImageColor set];
    NSRectFillUsingOperation(dirtyRect, operation);
    themed = YES;
  } else if (gradient) {
    NSPoint startPoint = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
    NSPoint endPoint = startPoint;
    endPoint.y -= kBrowserFrameViewPaintHeight;
    [gradient drawFromPoint:startPoint toPoint:endPoint options:0];
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
    NSView* frameView = [[[view window] contentView] superview];
    NSPoint position = [[view window] themeImagePositionForAlignment:
        THEME_IMAGE_ALIGN_WITH_FRAME];
    position = [view convertPoint:position fromView:frameView];
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

- (NSColor*)titleColor {
  ui::ThemeProvider* themeProvider = [self themeProvider];
  if (!themeProvider)
    return [NSColor windowFrameTextColor];

  ThemedWindowStyle windowStyle = [self themedWindowStyle];
  BOOL incognito = windowStyle & THEMED_INCOGNITO;

  if (incognito)
    return [NSColor whiteColor];
  else
    return [NSColor windowFrameTextColor];
}

@end
