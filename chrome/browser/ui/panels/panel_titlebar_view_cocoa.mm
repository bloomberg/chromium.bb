// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/base/theme_provider.h"

const int kRoundedCornerSize = 6;
const int kCloseButtonLeftPadding = 8;

@implementation PanelTitlebarViewCocoa

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
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
  [[self controller] closePanel];
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
}

- (void)attach {
  // Interface Builder can not put a view as a sibling of contentView,
  // so need to do it here. Placing the titleView as a last child of
  // internal view allows it to draw on top of the titlebar.
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
  NSRect contentFrame = [[[controller_ window] contentView] frame];
  NSRect titlebarFrame =
      NSMakeRect(NSMinX(contentFrame),
                 NSMaxY(contentFrame),
                 NSWidth(contentFrame),
                 NSMaxY(rootViewBounds) - NSMaxY(contentFrame));
  [self setFrame:titlebarFrame];

  // Update layout of controls in the titlebar.
  [self updateCloseButtonLayout];

  // Set autoresizing behavior: glued to edges on left, top and right.
  [self setAutoresizingMask:(NSViewMinYMargin | NSViewWidthSizable)];
}

- (void)setTitle:(NSString*)newTitle {
  // TODO(dcheng): Implement.
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
  NSWindow* panelWindow = [controller_ window];
  [closeButtonTrackingArea_.get() clearOwnerWhenWindowWillClose:panelWindow];
  [self addTrackingArea:closeButtonTrackingArea_.get()];
}

// PanelManager controls size/position of the window.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)mouseEntered:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:YES];
}

- (void)mouseExited:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:NO];
}

- (PanelWindowControllerCocoa*)controller {
  return controller_;
}

// (Private/TestingAPI)
- (void)simulateCloseButtonClick {
  [[closeButton_ cell] performClick:closeButton_];
}

@end

