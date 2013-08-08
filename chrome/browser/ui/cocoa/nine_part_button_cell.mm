// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/nine_part_button_cell.h"

#include <algorithm>

#include "ui/base/resource/resource_bundle.h"

@implementation NinePartButtonCell

- (id)initWithResourceIds:(const int[9])ids {
  if ((self = [super initImageCell:nil])) {
    images_.reset([[NSMutableArray alloc] initWithCapacity:9]);
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    for (NSInteger i = 0; i < 9; ++i)
      [images_ addObject:rb.GetNativeImageNamed(ids[i]).ToNSImage()];
    [self setBezeled:YES];
    [self setBezelStyle:NSSmallSquareBezelStyle];
  }
  return self;
}

- (void)drawBezelWithFrame:(NSRect)frame inView:(NSView*)controlView {
  NSSize targetSize = frame.size;

  // Retrieve the sizes of the images (except the size of the center image,
  // which will be scaled anyway).
  NSSize topLeft = [[images_ objectAtIndex:0] size];
  NSSize top = [[images_ objectAtIndex:1] size];
  NSSize topRight = [[images_ objectAtIndex:2] size];
  NSSize left = [[images_ objectAtIndex:3] size];
  NSSize right = [[images_ objectAtIndex:5] size];
  NSSize bottomLeft = [[images_ objectAtIndex:6] size];
  NSSize bottom = [[images_ objectAtIndex:7] size];
  NSSize bottomRight = [[images_ objectAtIndex:8] size];

  // Determine the minimum width of images on the left side.
  CGFloat minLeftWidth =
      std::min(topLeft.width, std::min(left.width, bottomLeft.width));
  // Determine the minimum width of images on the right side.
  CGFloat minRightWidth =
      std::min(topRight.width, std::min(right.width, bottomRight.width));
  // Determine the minimum height of images on the top side.
  CGFloat minTopHeight =
      std::min(topLeft.height, std::min(top.height, topRight.height));
  // Determine the minimum height of images on the bottom side.
  CGFloat minBottomHeight =
      std::min(bottomLeft.height, std::min(bottom.height, bottomRight.height));

  // Now paint the center image and extend it in all directions to the edges of
  // images with the smallest height/width.
  NSSize centerSize =
      NSMakeSize(targetSize.width - minLeftWidth - minRightWidth,
                 targetSize.height - minTopHeight - minBottomHeight);
  NSRect centerRect = NSMakeRect(
      minLeftWidth, minBottomHeight, centerSize.width, centerSize.height);
  [[images_ objectAtIndex:4] drawInRect:centerRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];

  // Paint the corner images next.
  NSRect topLeftRect = NSMakeRect(
      0, targetSize.height - topLeft.height, topLeft.width, topLeft.height);
  [[images_ objectAtIndex:0] drawInRect:topLeftRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
  NSRect topRightRect = NSMakeRect(targetSize.width - topRight.width,
                                   targetSize.height - topRight.height,
                                   topRight.width,
                                   topRight.height);
  [[images_ objectAtIndex:2] drawInRect:topRightRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
  NSRect bottomLeftRect = NSMakeRect(
      0, 0, bottomLeft.width, bottomLeft.height);
  [[images_ objectAtIndex:6] drawInRect:bottomLeftRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
  NSRect bottomRightRect = NSMakeRect(targetSize.width - bottomRight.width,
                                      0,
                                      bottomRight.width,
                                      bottomRight.height);
  [[images_ objectAtIndex:8] drawInRect:bottomRightRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];

  // Now paint the remaining images.
  NSRect topRect = NSMakeRect(minLeftWidth,
                              targetSize.height - top.height,
                              centerSize.width,
                              top.height);
  [[images_ objectAtIndex:1] drawInRect:topRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
  NSRect leftRect =
      NSMakeRect(0, minBottomHeight, left.width, centerSize.height);
  [[images_ objectAtIndex:3] drawInRect:leftRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
  NSRect rightRect = NSMakeRect(targetSize.width - minRightWidth,
                                minBottomHeight,
                                right.width,
                                centerSize.height);
  [[images_ objectAtIndex:5] drawInRect:rightRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
  NSRect bottomRect =
      NSMakeRect(minLeftWidth, 0, centerSize.width, bottom.height);
  [[images_ objectAtIndex:7] drawInRect:bottomRect
                               fromRect:NSZeroRect
                              operation:NSCompositeSourceOver
                               fraction:1.0];
}

@end
