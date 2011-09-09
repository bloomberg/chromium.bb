// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"

#include <Carbon/Carbon.h>  // kVK_Escape
#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/hover_image_button.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/mac/nsimage_cache.h"

const int kRoundedCornerSize = 6;
const int kCloseButtonLeftPadding = 8;

// Used to implement TestingAPI
static NSEvent* MakeMouseEvent(NSEventType type, NSPoint point) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:0
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:0
                            pressure:0.0];
}

@implementation PanelTitlebarViewCocoa

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    dragState_ = PANEL_DRAG_SUPPRESSED;
    // Create standard OSX Close Button.
    closeButton_ = [NSWindow standardWindowButton:NSWindowCloseButton
                                     forStyleMask:NSClosableWindowMask];
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(onCloseButtonClick:)];
    [self addSubview:closeButton_];
  }
  return self;
}

- (void)dealloc {
  if (closeButtonTrackingArea_.get())
    [self removeTrackingArea:closeButtonTrackingArea_.get()];
  [super dealloc];
}

- (void)onCloseButtonClick:(id)sender {
  [controller_ closePanel];
}

- (void)onSettingsButtonClick:(id)sender {
  [controller_ runSettingsMenu:settingsButton_];
}

- (void)drawRect:(NSRect)rect {
  NSBezierPath* path =
      [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                            topLeftCornerRadius:kRoundedCornerSize
                           topRightCornerRadius:kRoundedCornerSize
                         bottomLeftCornerRadius:0.0
                        bottomRightCornerRadius:0.0];
  [path addClip];
  NSPoint phase = [[self window] themePatternPhase];
  [[NSGraphicsContext currentContext] setPatternPhase:phase];
  [self drawBackgroundWithOpaque:YES];

  ui::ThemeProvider* theme = [[self window] themeProvider];
  NSColor* titleColor = nil;
  if (theme)
    titleColor = [[self window] isMainWindow]
        ? theme->GetNSColor(ThemeService::COLOR_TAB_TEXT, true)
        : theme->GetNSColor(ThemeService::COLOR_BACKGROUND_TAB_TEXT, true);
  else
    titleColor = [NSColor textColor];
  [title_ setTextColor:titleColor];
}

- (void)attach {
  // Interface Builder can not put a view as a sibling of contentView,
  // so need to do it here. Placing ourself as the last child of the
  // internal view allows us to draw on top of the titlebar.
  // Note we must use [controller_ window] here since we have not been added
  // to the view hierarchy yet.
  [[[[controller_ window] contentView] superview] addSubview:self];

  // Figure out the rectangle of the titlebar and set us on top of it.
  // The titlebar covers window's root view where not covered by contentView.
  // Compute the titlebar frame in coordinate system of the window's root view.
  //        NSWindow
  //           |
  //    ___root_view____
  //     |            |
  // contentView  titlebar
  NSRect rootViewBounds = [[self superview] bounds];
  NSRect contentFrame = [[[self window] contentView] frame];
  NSRect titlebarFrame =
      NSMakeRect(NSMinX(contentFrame),
                 NSMaxY(contentFrame),
                 NSWidth(contentFrame),
                 NSMaxY(rootViewBounds) - NSMaxY(contentFrame));
  [self setFrame:titlebarFrame];

  // Initialize the settings button.
  NSImage* image = gfx::GetCachedImageWithName(@"balloon_wrench.pdf");
  [settingsButton_ setDefaultImage:image];
  [settingsButton_ setDefaultOpacity:0.6];
  [settingsButton_ setHoverImage:image];
  [settingsButton_ setHoverOpacity:0.9];
  [settingsButton_ setPressedImage:image];
  [settingsButton_ setPressedOpacity:1.0];
  [[settingsButton_ cell] setHighlightsBy:NSNoCellMask];

  // Update layout of controls in the titlebar.
  [self updateCloseButtonLayout];

  // Set autoresizing behavior: glued to edges on left, top and right.
  [self setAutoresizingMask:(NSViewMinYMargin | NSViewWidthSizable)];

  // Register for various window focus changes, so we can update our custom
  // titlebar appropriately.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeMainWindow:)
             name:NSWindowDidBecomeMainNotification
           object:[self window]];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeMainWindow:)
             name:NSWindowDidResignMainNotification
           object:[self window]];
}

- (void)setTitle:(NSString*)newTitle {
  [title_ setStringValue:newTitle];
}

- (void)updateCloseButtonLayout {
  NSRect buttonBounds = [closeButton_ bounds];
  NSRect bounds = [self bounds];

  int x = kCloseButtonLeftPadding;
  int y = (NSHeight(bounds) - NSHeight(buttonBounds)) / 2;
  NSRect buttonFrame = NSMakeRect(x,
                                  y,
                                  NSWidth(buttonBounds),
                                  NSHeight(buttonBounds));
  [closeButton_ setFrame:buttonFrame];

  DCHECK(!closeButtonTrackingArea_.get());
  closeButtonTrackingArea_.reset(
      [[CrTrackingArea alloc] initWithRect:buttonFrame
                                   options:(NSTrackingMouseEnteredAndExited |
                                            NSTrackingActiveAlways)
                              proxiedOwner:self
                                  userInfo:nil]);
  NSWindow* panelWindow = [self window];
  [closeButtonTrackingArea_.get() clearOwnerWhenWindowWillClose:panelWindow];
  [self addTrackingArea:closeButtonTrackingArea_.get()];
}

// PanelManager controls size/position of the window.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}

- (void)mouseEntered:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:YES];
}

- (void)mouseExited:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:NO];
}

- (void)didChangeMainWindow:(NSNotification*)notification {
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
  dragState_ = PANEL_DRAG_CAN_START;
}

- (void)mouseDragged:(NSEvent*)event {
  // In addition to events needed to control the drag operation, fetch the right
  // mouse click events and key down events and ignore them, to prevent their
  // accumulation in the queue and "playing out" when the mouse is released.
  const NSUInteger mask =
      NSLeftMouseUpMask | NSLeftMouseDraggedMask | NSKeyUpMask |
      NSRightMouseDownMask | NSKeyDownMask ;
  BOOL keepGoing = YES;

  while (keepGoing) {
    base::mac::ScopedNSAutoreleasePool autorelease_pool;

    NSEvent* event = [NSApp nextEventMatchingMask:mask
                                        untilDate:[NSDate distantFuture]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];

    switch ([event type]) {
      case NSLeftMouseDragged:
        if (dragState_ == PANEL_DRAG_CAN_START)
          [self startDrag];
        [self dragWithDeltaX:[event deltaX]];
        break;

      case NSKeyUp:
        if ([event keyCode] == kVK_Escape) {
          [self endDrag:YES];
          keepGoing = NO;
        }
        break;

      case NSLeftMouseUp:
        [self endDrag:NO];
        keepGoing = NO;
        break;

      case NSRightMouseDownMask:
        break;

      default:
        // Dequeue and ignore other mouse and key events so the Chrome context
        // menu does not come after right click on a page during Panel
        // rearrangement, or the keystrokes are not 'accumulated' and entered
        // at once when the drag ends.
        break;
    }
  }
}

- (void)startDrag {
  DCHECK(dragState_ == PANEL_DRAG_CAN_START);
  dragState_ = PANEL_DRAG_IN_PROGRESS;
  [controller_ startDrag];
}

- (void)endDrag:(BOOL)cancelled {
  if (dragState_ == PANEL_DRAG_IN_PROGRESS)
    [controller_ endDrag:cancelled];
  dragState_ = PANEL_DRAG_SUPPRESSED;
}

- (void)dragWithDeltaX:(int)deltaX {
  if (dragState_ != PANEL_DRAG_IN_PROGRESS)
    return;
  [controller_ dragWithDeltaX:deltaX];
}

// (Private/TestingAPI)
- (PanelWindowControllerCocoa*)controller {
  return controller_;
}

- (void)simulateCloseButtonClick {
  [[closeButton_ cell] performClick:closeButton_];
}

- (void)pressLeftMouseButtonTitlebar {
  NSEvent* event = MakeMouseEvent(NSLeftMouseDown, NSZeroPoint);
  [self mouseDown:event];
}

- (void)releaseLeftMouseButtonTitlebar {
  NSEvent* event = MakeMouseEvent(NSLeftMouseUp, NSZeroPoint);
  [self mouseUp:event];
}

- (void)dragTitlebarDeltaX:(double)delta_x
                    deltaY:(double)delta_y {
  if (dragState_ == PANEL_DRAG_CAN_START)
    [self startDrag];
  [self dragWithDeltaX:delta_x];
}

- (void)cancelDragTitlebar {
  [self endDrag:YES];
}

- (void)finishDragTitlebar {
  [self endDrag:NO];
}

@end

