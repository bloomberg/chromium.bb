// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/cocoa/location_bar/image_decoration.h"

#import "chrome/browser/cocoa/image_utils.h"

ImageDecoration::ImageDecoration() {
}

ImageDecoration::~ImageDecoration() {
}

NSImage* ImageDecoration::GetImage() {
  return image_;
}

void ImageDecoration::SetImage(NSImage* image) {
  image_.reset([image retain]);
}

NSRect ImageDecoration::GetDrawRectInFrame(NSRect frame) {
  NSImage* image = GetImage();
  if (!image)
    return frame;

  const CGFloat delta_height = NSHeight(frame) - [image size].height;
  const CGFloat y_inset = std::floor(delta_height / 2.0);
  return NSInsetRect(frame, 0.0, y_inset);
}

CGFloat ImageDecoration::GetWidthForSpace(CGFloat width) {
  NSImage* image = GetImage();
  if (image) {
    const CGFloat image_width = [image size].width;
    if (image_width <= width)
      return image_width;
  }
  return kOmittedWidth;
}

void ImageDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  [GetImage() drawInRect:GetDrawRectInFrame(frame)
                fromRect:NSZeroRect  // Entire image
               operation:NSCompositeSourceOver
                fraction:1.0
            neverFlipped:YES];
}
