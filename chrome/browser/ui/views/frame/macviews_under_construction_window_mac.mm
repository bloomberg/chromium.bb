// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/macviews_under_construction_window_mac.h"

#import <objc/runtime.h>

namespace {
constexpr CGFloat kUnderConstructionWindowMinWidth = 40;
constexpr CGFloat kUnderConstructionWindowHeight = 15;
constexpr CGFloat kUnderConstructionWindowXInset = 10;
constexpr CGFloat kUnderConstructionWindowYInset = 5;
}  // namespace

@implementation MacViewsUnderConstructionWindow {
  NSView* stripesView_;  // weak
}

+ (void)attachToWindow:(NSWindow*)window {
  MacViewsUnderConstructionWindow* underConstructionWindow =
      objc_getAssociatedObject(window, _cmd);
  if (!underConstructionWindow) {
    underConstructionWindow = [[[self alloc] init] autorelease];
    objc_setAssociatedObject(window, _cmd, underConstructionWindow,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }

  [window addChildWindow:underConstructionWindow ordered:NSWindowBelow];
}

- (instancetype)init {
  if ((self = [super initWithContentRect:NSZeroRect
                               styleMask:NSWindowStyleMaskBorderless
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    self.opaque = NO;
    self.backgroundColor = NSColor.clearColor;
    self.hasShadow = YES;
    self.contentView.wantsLayer = YES;
    CALayer* layer = self.contentView.layer;

    layer.opaque = YES;
    layer.cornerRadius = 5;
    layer.backgroundColor = CGColorGetConstantColor(kCGColorWhite);
    layer.shadowColor = CGColorGetConstantColor(kCGColorBlack);
    layer.shadowRadius = 5;
    layer.shadowOpacity = 1;

    NSImage* stripesImage = [NSImage
         imageWithSize:NSMakeSize(40, 40)
               flipped:NO
        drawingHandler:^(NSRect rect) {
          [[NSColor colorWithCalibratedRed:1 green:0.8 blue:0 alpha:1] setFill];
          NSRectFill(rect);

          NSBezierPath* stripePath = [NSBezierPath bezierPath];
          [stripePath moveToPoint:NSMakePoint(0, 0)];
          [stripePath lineToPoint:NSMakePoint(40, 40)];
          [stripePath lineToPoint:NSMakePoint(40, 20)];
          [stripePath lineToPoint:NSMakePoint(20, 0)];
          [stripePath closePath];

          [NSColor.blackColor setFill];
          [stripePath fill];

          NSAffineTransform* shiftTransform = [NSAffineTransform transform];
          [shiftTransform translateXBy:-20 yBy:20];
          [stripePath transformUsingAffineTransform:shiftTransform];
          [stripePath fill];
          return YES;
        }];

    stripesView_ =
        [[[NSView alloc] initWithFrame:self.contentView.bounds] autorelease];
    stripesView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    stripesView_.wantsLayer = YES;
    stripesView_.layer.backgroundColor =
        [NSColor colorWithPatternImage:stripesImage].CGColor;

    [self.contentView addSubview:stripesView_];
  }
  return self;
}

- (void)setParentWindow:(NSWindow*)window {
  if (self.parentWindow) {
    [NSNotificationCenter.defaultCenter removeObserver:self];
  }

  [super setParentWindow:window];

  if (window) {
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(parentWindowDidChangeSize)
               name:NSWindowDidResizeNotification
             object:window];

    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(parentWindowDidChangeMain)
               name:NSWindowDidBecomeMainNotification
             object:window];

    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(parentWindowDidChangeMain)
               name:NSWindowDidResignMainNotification
             object:window];

    [self parentWindowDidChangeMain];
    [self parentWindowDidChangeSize];
  }
}

- (void)parentWindowDidChangeMain {
  stripesView_.alphaValue = self.parentWindow.isMainWindow ? 1 : 0.5;
}

- (void)parentWindowDidChangeSize {
  const NSRect parentFrame = self.parentWindow.frame;
  [self
      setFrame:NSMakeRect(NSMinX(parentFrame) + kUnderConstructionWindowXInset,
                          NSMaxY(parentFrame) - kUnderConstructionWindowYInset,
                          MAX(kUnderConstructionWindowMinWidth,
                              NSWidth(parentFrame) -
                                  kUnderConstructionWindowXInset * 2),
                          kUnderConstructionWindowHeight)
       display:NO];
}

@end
