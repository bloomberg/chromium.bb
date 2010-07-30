// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tabpose_window.h"

#import <QuartzCore/QuartzCore.h>

const int kTopGradientHeight  = 15;

// CAGradientLayer is 10.6-only -- roll our own.
@interface DarkGradientLayer : CALayer
- (void)drawInContext:(CGContextRef)context;
@end

@implementation DarkGradientLayer
- (void)drawInContext:(CGContextRef)context {
  scoped_cftyperef<CGColorSpaceRef> grayColorSpace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericGray));
  CGFloat grays[] = { 0.277, 1.0, 0.39, 1.0 };
  CGFloat locations[] = { 0, 1 };
  scoped_cftyperef<CGGradientRef> gradient(CGGradientCreateWithColorComponents(
      grayColorSpace.get(), grays, locations, arraysize(locations)));
  CGPoint topLeft = CGPointMake(0.0, kTopGradientHeight);
  CGContextDrawLinearGradient(context, gradient.get(), topLeft, CGPointZero, 0);
}
@end

@interface TabposeWindow (Private)
- (id)initForWindow:(NSWindow*)parent rect:(NSRect)rect slomo:(BOOL)slomo;
- (void)setUpLayers:(NSRect)bgLayerRect;
@end

@implementation TabposeWindow

+ (id)openTabposeFor:(NSWindow*)parent rect:(NSRect)rect slomo:(BOOL)slomo {
  // Releases itself when closed.
  return [[TabposeWindow alloc] initForWindow:parent rect:rect slomo:slomo];
}

- (id)initForWindow:(NSWindow*)parent rect:(NSRect)rect slomo:(BOOL)slomo {
  NSRect frame = [parent frame];
  if ((self = [super initWithContentRect:frame
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    [self setReleasedWhenClosed:YES];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setUpLayers:rect];
    [parent addChildWindow:self ordered:NSWindowAbove];
    [self makeKeyAndOrderFront:self];
  }
  return self;
}

- (void)setUpLayers:(NSRect)bgLayerRect {
  // Root layer -- covers whole window.
  rootLayer_ = [CALayer layer];
  [[self contentView] setLayer:rootLayer_];
  [[self contentView] setWantsLayer:YES];

  // Background layer -- the visible part of the window.
  gray_.reset(CGColorCreateGenericGray(0.39, 1.0));
  bgLayer_ = [CALayer layer];
  bgLayer_.backgroundColor = gray_;
  bgLayer_.frame = NSRectToCGRect(bgLayerRect);
  bgLayer_.masksToBounds = YES;
  [rootLayer_ addSublayer:bgLayer_];

  // Top gradient.
  CALayer* gradientLayer = [DarkGradientLayer layer];
  gradientLayer.frame = CGRectMake(
      0,
      NSHeight(bgLayerRect) - kTopGradientHeight,
      NSWidth(bgLayerRect),
      kTopGradientHeight);
  [gradientLayer setNeedsDisplay];  // Draw once.
  [bgLayer_ addSublayer:gradientLayer];
}

- (BOOL)canBecomeKeyWindow {
 return YES;
}

- (void)keyDown:(NSEvent*)event {
  // Overridden to prevent beeps.
}

- (void)keyUp:(NSEvent*)event {
  NSString* characters = [event characters];
  if ([characters length] < 1)
    return;

  unichar character = [characters characterAtIndex:0];
  switch (character) {
    case NSEnterCharacter:
    case NSNewlineCharacter:
    case NSCarriageReturnCharacter:
    case ' ':
    case '\e':  // Escape
      [self close];
      break;
  }
}

- (void)mouseDown:(NSEvent*)event {
 [self close];
}

- (void)swipeWithEvent:(NSEvent*)event {
  if ([event deltaY] > 0.5)  // Swipe up
    [self close];
}

- (void)close {
  // Prevent parent window from disappearing.
  [[self parentWindow] removeChildWindow:self];
  [super close];
}

- (void)commandDispatch:(id)sender {
  // Without this, -validateUserInterfaceItem: is not called.
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // Disable all browser-related menu items.
  return NO;
}

@end
