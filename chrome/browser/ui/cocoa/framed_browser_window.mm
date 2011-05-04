// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/framed_browser_window.h"

#include "base/logging.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#import "chrome/browser/ui/cocoa/browser_frame_view.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/themes/theme_service.h"

// Implementer's note: Moving the window controls is tricky. When altering the
// code, ensure that:
// - accessibility hit testing works
// - the accessibility hierarchy is correct
// - close/min in the background don't bring the window forward
// - rollover effects work correctly

namespace {

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

@implementation FramedBrowserWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:aStyle
                                 backing:bufferingType
                                   defer:flag])) {
    if (aStyle & NSTexturedBackgroundWindowMask) {
      // The following two calls fix http://www.crbug.com/25684 by preventing
      // the window from recalculating the border thickness as the window is
      // resized.
      // This was causing the window tint to change for the default system theme
      // when the window was being resized.
      [self setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
      [self setContentBorderThickness:kWindowGradientHeight forEdge:NSMaxYEdge];
    }

    closeButton_ = [self standardWindowButton:NSWindowCloseButton];
    [closeButton_ setPostsFrameChangedNotifications:YES];
    miniaturizeButton_ = [self standardWindowButton:NSWindowMiniaturizeButton];
    [miniaturizeButton_ setPostsFrameChangedNotifications:YES];
    zoomButton_ = [self standardWindowButton:NSWindowZoomButton];
    [zoomButton_ setPostsFrameChangedNotifications:YES];

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

- (void)setWindowController:(NSWindowController*)controller {
  if (controller == [self windowController]) {
    return;
  }

  [super setWindowController:controller];

  BrowserWindowController* browserController
      = static_cast<BrowserWindowController*>(controller);
  if ([browserController isKindOfClass:[BrowserWindowController class]]) {
    hasTabStrip_ = [browserController hasTabStrip];
  } else {
    hasTabStrip_ = NO;
  }

  // Force re-layout of the window buttons by wiggling the size of the frame
  // view.
  NSView* frameView = [[self contentView] superview];
  BOOL frameViewDidAutoresizeSubviews = [frameView autoresizesSubviews];
  [frameView setAutoresizesSubviews:NO];
  NSRect oldFrame = [frameView frame];
  [frameView setFrame:NSZeroRect];
  [frameView setFrame:oldFrame];
  [frameView setAutoresizesSubviews:frameViewDidAutoresizeSubviews];
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
      buttonFrame.origin.x += kFramedWindowButtonsInterButtonSpacing;
      // fallthrough
    case NSWindowMiniaturizeButton:
      buttonFrame.origin.x += NSWidth([closeButton_ frame]);
      buttonFrame.origin.x += kFramedWindowButtonsInterButtonSpacing;
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

@end
