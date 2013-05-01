// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"

#include <algorithm>

#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

const CGFloat kGap = 6.0;  // gap between icon and text.

}  // namespace

@interface AutofillTextFieldCell (Internal)

- (NSRect)iconFrameForFrame:(NSRect)frame;
- (NSRect)textFrameForFrame:(NSRect)frame;

@end

@implementation AutofillTextField

+ (Class)cellClass {
  return [AutofillTextFieldCell class];
}

@end

@implementation AutofillTextFieldCell

@synthesize invalid = invalid_;

- (NSImage*) icon{
  return icon_;
}

- (void)setIcon:(NSImage*) icon {
  icon_.reset([icon retain]);
}

- (NSRect)textFrameForFrame:(NSRect)frame {
  if (icon_) {
    NSRect textFrame, iconFrame;
    NSDivideRect(frame, &iconFrame, &textFrame,
                 kGap + [icon_ size].width, NSMaxXEdge);
    return textFrame;
  }
  return frame;
}

- (NSRect)iconFrameForFrame:(NSRect)frame {
  NSRect iconFrame;
  if (icon_) {
    NSRect textFrame;
    NSDivideRect(frame, &iconFrame, &textFrame,
                 kGap + [icon_ size].width, NSMaxXEdge);
  }
  return iconFrame;
}

- (NSSize)cellSize {
  NSSize cellSize = [super cellSize];

  if (icon_) {
    NSSize iconSize = [icon_ size];
    cellSize.width += kGap + iconSize.width;
    cellSize.height = std::max(cellSize.height, iconSize.height);
  }

  return cellSize;
}

- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
               editor:(NSText *)editor
             delegate:(id)delegate
                event:(NSEvent *)event {
  [super editWithFrame:[self textFrameForFrame:cellFrame]
                inView:controlView
                editor:editor
              delegate:delegate
                 event:event];
}

- (void)selectWithFrame:(NSRect)cellFrame
                 inView:(NSView *)controlView
                 editor:(NSText *)editor
               delegate:(id)delegate
                  start:(NSInteger)start
                 length:(NSInteger)length {
  [super selectWithFrame:[self textFrameForFrame:cellFrame]
                  inView:controlView
                  editor:editor
                delegate:delegate
                   start:start
                  length:length];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [super drawWithFrame:cellFrame inView:controlView];

  if (icon_) {
    NSRect iconFrame = [self iconFrameForFrame:cellFrame];
    iconFrame.size = [icon_ size];
    iconFrame.origin.y +=
        roundf((NSHeight(cellFrame) - NSHeight(iconFrame)) / 2.0);
    [icon_ drawInRect:iconFrame
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:YES
                hints:nil];
  }

  if (invalid_) {
    gfx::ScopedNSGraphicsContextSaveGState state;

    // Render red border for invalid fields.
    [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] setStroke];
    [[NSBezierPath bezierPathWithRect:NSInsetRect(cellFrame, 0.5, 0.5)] stroke];

    // Render a dog ear to flag invalid fields.
    const CGFloat kDogEarSize = 10.0f;

    // TODO(groby): This is a temporary placeholder and will be replaced
    // with an image. (Pending UI/UX work).
    [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] setFill];
    NSBezierPath* dog_ear = [NSBezierPath bezierPath];
    NSPoint corner = NSMakePoint(NSMaxX(cellFrame), NSMinY(cellFrame));
    [dog_ear moveToPoint:NSMakePoint(corner.x - kDogEarSize, corner.y)];
    [dog_ear lineToPoint:corner];
    [dog_ear lineToPoint:NSMakePoint(corner.x, corner.y + kDogEarSize)];
    [dog_ear closePath];
    [dog_ear fill];
  }
}

@end
