// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_overlay_controller.h"

#import <QuartzCore/QuartzCore.h>

#include <cmath>

// OverlayFrameView ////////////////////////////////////////////////////////////

// The content view of the window that draws a custom frame.
@interface OverlayFrameView : NSView {
 @private
  NSTextField* message_;  // Weak, owned by the view hierarchy.
}
- (void)setMessageText:(NSString*)text;
@end

// The content view of the window that draws a custom frame.
@implementation OverlayFrameView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    scoped_nsobject<NSTextField> message(
        // The frame will be fixed up when |-setMessageText:| is called.
        [[NSTextField alloc] initWithFrame:NSZeroRect]);
    message_ = message.get();
    [message_ setEditable:NO];
    [message_ setSelectable:NO];
    [message_ setBezeled:NO];
    [message_ setDrawsBackground:NO];
    [message_ setFont:[NSFont boldSystemFontOfSize:72]];
    [message_ setTextColor:[NSColor whiteColor]];
    [self addSubview:message_];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  const CGFloat kCornerRadius = 5.0;
  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                                       xRadius:kCornerRadius
                                                       yRadius:kCornerRadius];

  NSColor* fillColor = [NSColor colorWithCalibratedWhite:0.2 alpha:0.85];
  [fillColor set];
  [path fill];
}

- (void)setMessageText:(NSString*)text {
  const CGFloat kHorizontalPadding = 30;  // In view coordinates.

  // Style the string.
  scoped_nsobject<NSMutableAttributedString> attrString(
      [[NSMutableAttributedString alloc] initWithString:text]);
  scoped_nsobject<NSShadow> textShadow([[NSShadow alloc] init]);
  [textShadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0
                                                               alpha:0.6]];
  [textShadow.get() setShadowOffset:NSMakeSize(0, -1)];
  [textShadow setShadowBlurRadius:1.0];
  [attrString addAttribute:NSShadowAttributeName
                     value:textShadow
                     range:NSMakeRange(0, [text length])];
  [message_ setAttributedStringValue:attrString];

  // Fix up the frame of the string.
  [message_ sizeToFit];
  NSRect messageFrame = [message_ frame];
  NSRect frameInViewSpace =
      [message_ convertRect:[[self window] frame] fromView:nil];

  if (NSWidth(messageFrame) > NSWidth(frameInViewSpace))
    frameInViewSpace.size.width = NSWidth(messageFrame) + kHorizontalPadding;

  messageFrame.origin.x =
      (NSWidth(frameInViewSpace) - NSWidth(messageFrame)) / 2;
  messageFrame.origin.y =
      (NSHeight(frameInViewSpace) - NSHeight(messageFrame)) / 2;

  [[self window] setFrame:[message_ convertRect:frameInViewSpace toView:nil]
                  display:YES];
  [message_ setFrame:messageFrame];
}

@end

// HistoryOverlayController ////////////////////////////////////////////////////

@implementation HistoryOverlayController

- (id)initForMode:(HistoryOverlayMode)mode {
  const NSRect kWindowFrame = NSMakeRect(0, 0, 120, 70);
  scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:kWindowFrame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  if ((self = [super initWithWindow:window])) {
    mode_ = mode;

    [window setDelegate:self];
    [window setBackgroundColor:[NSColor clearColor]];
    [window setOpaque:NO];
    [window setHasShadow:NO];

    // Create the content view. Take the frame from the existing content view.
    NSRect frame = [[window contentView] frame];
    scoped_nsobject<OverlayFrameView> frameView(
        [[OverlayFrameView alloc] initWithFrame:frame]);
    contentView_ = frameView.get();
    [window setContentView:contentView_];

    const unichar kBackArrowCharacter = 0x2190;
    const unichar kForwardArrowCharacter = 0x2192;

    unichar commandChar = mode_ == kHistoryOverlayModeForward ?
        kForwardArrowCharacter : kBackArrowCharacter;
    NSString* text =
        [NSString stringWithCharacters:&commandChar length:1];
    [contentView_ setMessageText:text];
  }
  return self;
}

- (void)setProgress:(CGFloat)gestureAmount {
  const CGFloat kVerticalPositionRatio = 0.65;
  NSRect windowFrame = [parent_ frame];
  CGFloat minX = NSMinX(windowFrame);
  CGFloat maxX = NSMaxX(windowFrame) - NSWidth([[self window] frame]);
  CGFloat x = 0;
  if (mode_ == kHistoryOverlayModeForward)
    x = maxX + gestureAmount * (maxX - minX);
  else if (mode_ == kHistoryOverlayModeBack)
    x = minX + gestureAmount * (maxX - minX);
  NSPoint p = [parent_ frame].origin;
  p.x = x;
  p.y += (NSHeight(windowFrame) - NSHeight([[self window] frame])) *
         kVerticalPositionRatio;
  [[self window] setFrameOrigin:p];

  CGFloat alpha =
      -(std::abs(gestureAmount) - 1) * (std::abs(gestureAmount) - 1) + 1;
  [[self window] setAlphaValue:alpha];
}

- (void)dismiss {
  const CGFloat kFadeOutDurationSeconds = 0.2;

  NSWindow* overlayWindow = [self window];

  scoped_nsobject<CAAnimation> animation(
      [[overlayWindow animationForKey:@"alphaValue"] copy]);
  [animation setDelegate:self];
  [animation setDuration:kFadeOutDurationSeconds];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithCapacity:1];
  [dictionary setObject:animation forKey:@"alphaValue"];
  [overlayWindow setAnimations:dictionary];
  [[overlayWindow animator] setAlphaValue:0.0];
}

- (void)windowWillClose:(NSNotification*)notification {
  // Release all animations because CAAnimation retains its delegate (self),
  // which will cause a retain cycle. Break it!
  [[self window] setAnimations:[NSDictionary dictionary]];
}

- (void)showPanelForWindow:(NSWindow*)window {
  parent_.reset([window retain]);
  [self setProgress:0];  // Set initial window position.
  [self showWindow:self];
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)finished {
  [self close];
}

@end
