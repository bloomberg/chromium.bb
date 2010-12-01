// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/image_utils.h"

@implementation NSImage (FlippedAdditions)

- (void)drawInRect:(NSRect)dstRect
          fromRect:(NSRect)srcRect
         operation:(NSCompositingOperation)op
          fraction:(CGFloat)requestedAlpha
      neverFlipped:(BOOL)neverFlipped {
  NSAffineTransform *transform = nil;

  // Flip drawing and adjust the origin to make the image come out
  // right.
  if (neverFlipped && [[NSGraphicsContext currentContext] isFlipped]) {
    transform = [NSAffineTransform transform];
    [transform scaleXBy:1.0 yBy:-1.0];
    [transform concat];

    // The lower edge of the image is as far from the origin as the
    // upper edge was, plus it's on the other side of the origin.
    dstRect.origin.y -= NSMaxY(dstRect) + NSMinY(dstRect);
  }

  [self drawInRect:dstRect
          fromRect:srcRect
         operation:op
          fraction:requestedAlpha];

  // Flip drawing back, if needed.
  [transform concat];
}

@end
