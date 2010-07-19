// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/bubble_decoration.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/image_utils.h"

namespace {

// Padding between the icon/label and bubble edges.
const CGFloat kBubblePadding = 7.0;

// How far to inset the keywork token from sides.
const NSInteger kKeywordYInset = 4;

}  // namespace

BubbleDecoration::BubbleDecoration(NSFont* font) {
  DCHECK(font);
  if (font) {
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObject:font
                                    forKey:NSFontAttributeName];
    attributes_.reset([attributes retain]);
  }
}

BubbleDecoration::~BubbleDecoration() {
}

CGFloat BubbleDecoration::GetWidthForImageAndLabel(NSImage* image,
                                                   NSString* label) {
  if (!image && !label)
    return kOmittedWidth;

  const CGFloat image_width = image ? [image size].width : 0.0;
  if (!label)
    return kBubblePadding + image_width;

  const CGFloat label_width = [label sizeWithAttributes:attributes_].width;
  return kBubblePadding + image_width + label_width;
}

NSRect BubbleDecoration::GetImageRectInFrame(NSRect frame) {
  NSRect imageRect = NSInsetRect(frame, 0.0, kKeywordYInset);
  if (image_)
    imageRect.size = [image_ size];
  return imageRect;
}

CGFloat BubbleDecoration::GetWidthForSpace(CGFloat width) {
  const CGFloat all_width = GetWidthForImageAndLabel(image_, label_);
  if (all_width <= width)
    return all_width;

  const CGFloat image_width = GetWidthForImageAndLabel(image_, nil);
  if (image_width <= width)
    return image_width;

  return kOmittedWidth;
}

void BubbleDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  const NSRect decorationFrame = NSInsetRect(frame, 0.0, kKeywordYInset);

  const NSRect bubbleFrame = NSInsetRect(decorationFrame, 0.5, 0.5);
  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:bubbleFrame
                                      xRadius:4.0
                                      yRadius:4.0];

  [background_color_ setFill];
  [path fill];

  [border_color_ setStroke];
  [path setLineWidth:1.0];
  [path stroke];

  NSRect imageRect = decorationFrame;
  if (image_) {
    imageRect.size = [image_ size];
    [image_ drawInRect:imageRect
              fromRect:NSZeroRect  // Entire image
             operation:NSCompositeSourceOver
              fraction:1.0
          neverFlipped:YES];
  } else {
    imageRect.size = NSZeroSize;
  }

  if (label_) {
    NSRect textRect = decorationFrame;
    textRect.origin.x = NSMaxX(imageRect);
    textRect.size.width = NSMaxX(decorationFrame) - NSMinX(textRect);
    [text_color_ set];
    [label_ drawInRect:textRect withAttributes:attributes_];
  }
}

void BubbleDecoration::SetImage(NSImage* image) {
  image_.reset([image retain]);
}

void BubbleDecoration::SetLabel(NSString* label) {
  // If the initializer was called with |nil|, then the code cannot
  // process a label.
  DCHECK(attributes_);
  if (attributes_)
    label_.reset([label copy]);
}

void BubbleDecoration::SetColors(NSColor* border_color,
                                 NSColor* background_color,
                                 NSColor* text_color) {
  border_color_.reset([border_color retain]);
  background_color_.reset([background_color retain]);
  text_color_.reset([text_color retain]);
}
