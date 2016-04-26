// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/exclusive_access_bubble_view.h"

#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

const CGFloat kShadowTop = 20;
const CGFloat kShadowBottom = 50;
const CGFloat kShadowLeft = 50;
const CGFloat kShadowRight = 50;
const CGFloat kShadowBlurRadius = 30;
const CGFloat kShadowAlpha = 0.5;
const CGFloat kBubbleCornerRadius = 8.0;
}

@implementation ExclusiveAccessBubbleView

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  bounds.size.width -= kShadowLeft + kShadowRight;
  bounds.size.height -= kShadowTop + kShadowBottom;
  bounds.origin.x += kShadowLeft;
  bounds.origin.y += kShadowBottom;
  NSBezierPath* bezier = [NSBezierPath bezierPath];

  CGFloat radius = kBubbleCornerRadius;
  // Start with a rounded rectangle.
  [bezier appendBezierPathWithRoundedRect:bounds xRadius:radius yRadius:radius];

  [bezier closePath];
  [[NSColor whiteColor] set];
  gfx::ScopedNSGraphicsContextSaveGState scoped_g_state;
  base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow setShadowBlurRadius:kShadowBlurRadius];
  [shadow setShadowColor:[[NSColor blackColor]
                             colorWithAlphaComponent:kShadowAlpha]];
  [shadow set];

  [bezier fill];
}

@end
