// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/bubble_decoration.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/material_design/material_design_controller.h"

namespace {

// This is used to increase the right margin of this decoration.
const CGFloat kRightSideMargin = 1.0;

// Padding between the icon/label and bubble edges.
CGFloat BubblePadding() {
  return ui::MaterialDesignController::IsModeMaterial() ? 7 : 3;
}


// Padding between the icon and label.
const CGFloat kIconLabelPadding = 4.0;

// Inset for the background.
const CGFloat kBackgroundYInset = 4.0;

}  // namespace

BubbleDecoration::BubbleDecoration() : baseline_offset_(0) {
  attributes_.reset([[NSMutableDictionary alloc] init]);
  [attributes_ setObject:LocationBarDecoration::GetFont()
                  forKey:NSFontAttributeName];
}

BubbleDecoration::~BubbleDecoration() {
}

CGFloat BubbleDecoration::GetWidthForImageAndLabel(NSImage* image,
                                                   NSString* label) {
  if (!image && !label)
    return kOmittedWidth;

  const CGFloat image_width = image ? [image size].width : 0.0;
  if (!label)
    return BubblePadding() + image_width;

  // The bubble needs to take up an integral number of pixels.
  // Generally -sizeWithAttributes: seems to overestimate rather than
  // underestimate, so floor() seems to work better.
  const CGFloat label_width =
      std::floor([label sizeWithAttributes:attributes_].width);
  return BubblePadding() + image_width + kIconLabelPadding + label_width;
}

NSRect BubbleDecoration::GetImageRectInFrame(NSRect frame) {
  NSRect imageRect = NSInsetRect(frame, 0.0, kBackgroundYInset);
  if (image_) {
    // Center the image vertically.
    const NSSize imageSize = [image_ size];
    imageRect.origin.y +=
        std::floor((NSHeight(frame) - imageSize.height) / 2.0);
    imageRect.size = imageSize;
  }
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
  const NSRect decoration_frame = NSInsetRect(frame, 0.0, kBackgroundYInset);
  CGFloat textOffset = NSMinX(decoration_frame);
  if (image_) {
    // Center the image vertically.
    const NSSize imageSize = [image_ size];
    NSRect imageRect = decoration_frame;
    imageRect.origin.y +=
        std::floor((NSHeight(decoration_frame) - imageSize.height) / 2.0);
    imageRect.size = imageSize;
    [image_ drawInRect:imageRect
              fromRect:NSZeroRect  // Entire image
             operation:NSCompositeSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
    textOffset = NSMaxX(imageRect) + kIconLabelPadding;
  }

  if (label_) {
    NSRect textRect = frame;
    textRect.origin.x = textOffset;
    textRect.origin.y += baseline_offset_;
    textRect.size.width = NSMaxX(decoration_frame) - NSMinX(textRect);
    DrawLabel(label_, attributes_, textRect);
  }
}

void BubbleDecoration::DrawWithBackgroundInFrame(NSRect background_frame,
                                                 NSRect frame,
                                                 NSView* control_view) {
  NSRect rect = NSInsetRect(background_frame, 0, 1);
  rect.size.width -= kRightSideMargin;
  if (ui::MaterialDesignController::IsModeMaterial()) {
    CGFloat lineWidth = [control_view cr_lineWidth];
    rect = NSInsetRect(rect, lineWidth / 2., lineWidth / 2.);
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:rect
                                                         xRadius:3
                                                         yRadius:3];
    [path setLineWidth:lineWidth];
    bool inDarkMode = [[control_view window] inIncognitoModeWithSystemTheme];
    if (inDarkMode) {
      [[NSColor whiteColor] set];
      [path fill];
    } else {
      [GetBackgroundBorderColor() set];
      [path stroke];
    }
  } else {
    ui::DrawNinePartImage(
        rect, GetBubbleImageIds(), NSCompositeSourceOver, 1.0, true);
  }

  DrawInFrame(frame, control_view);
}

NSFont* BubbleDecoration::GetFont() const {
  return [attributes_ objectForKey:NSFontAttributeName];
}

NSImage* BubbleDecoration::GetImage() {
  return image_;
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

void BubbleDecoration::SetTextColor(NSColor* text_color) {
  [attributes_ setObject:text_color forKey:NSForegroundColorAttributeName];
}

void BubbleDecoration::SetFont(NSFont* font) {
  [attributes_ setObject:font forKey:NSFontAttributeName];
}

void BubbleDecoration::SetBaselineOffset(CGFloat offset) {
  baseline_offset_ = offset;
}
