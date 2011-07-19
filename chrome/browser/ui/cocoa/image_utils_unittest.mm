// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface ImageUtilsTestView : NSView {
 @private
  // Determine whether the view is flipped.
  BOOL isFlipped_;

  // Determines whether to draw using the new method with
  // |neverFlipped:|.
  BOOL useNeverFlipped_;

  // Passed to |neverFlipped:| when drawing |image_|.
  BOOL neverFlipped_;

  scoped_nsobject<NSImage> image_;
}
@property(assign, nonatomic) BOOL isFlipped;
@property(assign, nonatomic) BOOL useNeverFlipped;
@property(assign, nonatomic) BOOL neverFlipped;
@end

@implementation ImageUtilsTestView
@synthesize isFlipped = isFlipped_;
@synthesize useNeverFlipped = useNeverFlipped_;
@synthesize neverFlipped = neverFlipped_;

- (id)initWithFrame:(NSRect)rect {
  self = [super initWithFrame:rect];
  if (self) {
    rect = NSInsetRect(rect, 5.0, 5.0);
    rect.origin = NSZeroPoint;
    const NSSize imageSize = NSInsetRect(rect, 5.0, 5.0).size;
    image_.reset([[NSImage alloc] initWithSize:imageSize]);

    NSBezierPath* path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(NSMinX(rect), NSMinY(rect))];
    [path lineToPoint:NSMakePoint(NSMinX(rect), NSMaxY(rect))];
    [path lineToPoint:NSMakePoint(NSMaxX(rect), NSMinY(rect))];
    [path closePath];

    [image_ lockFocus];
    [[NSColor blueColor] setFill];
    [path fill];
    [image_ unlockFocus];
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:NSMakePoint(NSMinX(rect), NSMinY(rect))];
  [path lineToPoint:NSMakePoint(NSMinX(rect), NSMaxY(rect))];
  [path lineToPoint:NSMakePoint(NSMaxX(rect), NSMinY(rect))];
  [path closePath];

  [[NSColor redColor] setFill];
  [path fill];

  rect = NSInsetRect(rect, 5.0, 5.0);
  rect = NSOffsetRect(rect, 2.0, 2.0);

  if (useNeverFlipped_) {
    [image_ drawInRect:rect
              fromRect:NSZeroRect
             operation:NSCompositeCopy
              fraction:1.0
          neverFlipped:neverFlipped_];
  } else {
    [image_ drawInRect:rect
              fromRect:NSZeroRect
             operation:NSCompositeCopy
              fraction:1.0];
  }
}

@end

namespace {

class ImageUtilTest : public CocoaTest {
 public:
  ImageUtilTest() {
    const NSRect frame = NSMakeRect(0, 0, 300, 100);
    scoped_nsobject<ImageUtilsTestView> view(
        [[ImageUtilsTestView alloc] initWithFrame: frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  NSData* SnapshotView() {
    [view_ display];

    const NSRect bounds = [view_ bounds];

    [view_ lockFocus];
    scoped_nsobject<NSBitmapImageRep> bitmap(
        [[NSBitmapImageRep alloc] initWithFocusedViewRect:bounds]);
    [view_ unlockFocus];

    return [bitmap TIFFRepresentation];
  }

  NSData* SnapshotViewBase() {
    [view_ setUseNeverFlipped:NO];
    return SnapshotView();
  }

  NSData* SnapshotViewNeverFlipped(BOOL neverFlipped) {
    [view_ setUseNeverFlipped:YES];
    [view_ setNeverFlipped:neverFlipped];
    return SnapshotView();
  }

  ImageUtilsTestView* view_;
};

TEST_F(ImageUtilTest, Test) {
  // When not flipped, both drawing methods return the same data.
  [view_ setIsFlipped:NO];
  NSData* baseSnapshotData = SnapshotViewBase();
  EXPECT_TRUE([baseSnapshotData isEqualToData:SnapshotViewNeverFlipped(YES)]);
  EXPECT_TRUE([baseSnapshotData isEqualToData:SnapshotViewNeverFlipped(NO)]);

  // When flipped, there's only a difference when the context flip is
  // not being respected.
  [view_ setIsFlipped:YES];
  baseSnapshotData = SnapshotViewBase();
  EXPECT_FALSE([baseSnapshotData isEqualToData:SnapshotViewNeverFlipped(YES)]);
  EXPECT_TRUE([baseSnapshotData isEqualToData:SnapshotViewNeverFlipped(NO)]);
}

}  // namespace
