// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/notifications/balloon_view.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/scoped_nsobject.h"

namespace {

const int kRoundedCornerSize = 4;

}  // namespace


@implementation BalloonViewCocoa
- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  NSBezierPath* path =
    [NSBezierPath bezierPathWithRoundedRect:bounds
                                    xRadius:kRoundedCornerSize
                                    yRadius:kRoundedCornerSize];
  [[NSColor lightGrayColor] set];
  [path fill];
  [[NSColor blackColor] set];
  [path stroke];
}
@end

@implementation BalloonShelfViewCocoa
- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  NSBezierPath* path =
    [NSBezierPath bezierPathWithRoundedRect:bounds
                                    xRadius:kRoundedCornerSize
                                    yRadius:kRoundedCornerSize];
  [[NSColor colorWithCalibratedRed:0.304 green:0.549 blue:0.85 alpha:1.0] set];
  [path fill];
  [[NSColor blackColor] set];
  [path stroke];
}
@end

@implementation BalloonButtonCell

- (id)initTextCell:(NSString*)string {
  if ((self = [super initTextCell:string])) {
    [self setButtonType:NSMomentaryPushInButton];
    [self setShowsBorderOnlyWhileMouseInside:YES];
    [self setBezelStyle:NSShadowlessSquareBezelStyle];
    [self setControlSize:NSSmallControlSize];
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  }

  return self;
}

- (void)setTextColor:(NSColor*)color {
  NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
      color, NSForegroundColorAttributeName,
      [self font], NSFontAttributeName, nil];
  scoped_nsobject<NSAttributedString> title(
      [[NSAttributedString alloc] initWithString:[self title] attributes:dict]);

  NSButton* button = static_cast<NSButton*>([self controlView]);
  DCHECK([button isKindOfClass:[NSButton class]]);
  [button setAttributedTitle:title.get()];
}
@end
