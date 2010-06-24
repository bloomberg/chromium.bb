// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/notifications/balloon_view.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"

namespace {

const int kRoundedCornerSize = 6.5;

}  // namespace

@implementation BalloonWindow
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(unsigned int)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  self = [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask
                            backing:NSBackingStoreBuffered
                              defer:NO];
  if (self) {
    [self setLevel:NSStatusWindowLevel];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
  }
  return self;
}
@end

@implementation BalloonShelfViewCocoa
- (void)drawRect:(NSRect)rect {
  NSBezierPath* path =
      [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                            topLeftCornerRadius:kRoundedCornerSize
                           topRightCornerRadius:kRoundedCornerSize
                         bottomLeftCornerRadius:0.0
                        bottomRightCornerRadius:0.0];

  [[NSColor colorWithCalibratedWhite:0.957 alpha:1.0] set];
  [path fill];
  [[NSColor blackColor] set];
  [path stroke];
}
@end

@implementation BalloonContentViewCocoa
- (void)drawRect:(NSRect)rect {
  NSBezierPath* path =
      [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                            topLeftCornerRadius:0.0
                           topRightCornerRadius:0.0
                         bottomLeftCornerRadius:kRoundedCornerSize
                        bottomRightCornerRadius:kRoundedCornerSize];
  [[NSColor blackColor] set];
  [path stroke];
}
@end
