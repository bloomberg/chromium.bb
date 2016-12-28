// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/bubble_decoration.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// This is used to increase the left padding of this decoration.
const CGFloat kLeftSidePadding = 5.0;

// Padding between the icon/label and bubble edges.
const CGFloat kBubblePadding = 7.0;

// Additional padding between the divider and the label.
const CGFloat kDividerPadding = 6.0;

// Padding between the icon and label.
const CGFloat kIconLabelPadding = 4.0;

// Inset for the image frame.
const CGFloat kImageFrameYInset = 4.0;

// Inset for the background frame.
const CGFloat kBackgroundFrameYInset = 2.0;

// Left margin for the background frame.
const CGFloat kBackgroundFrameLeftMargin = 1.0;

}  // namespace

BubbleDecoration::BubbleDecoration() : retina_baseline_offset_(0) {
  attributes_.reset([[NSMutableDictionary alloc] init]);
  [attributes_ setObject:LocationBarDecoration::GetFont()
                  forKey:NSFontAttributeName];
}

BubbleDecoration::~BubbleDecoration() {
}

CGFloat BubbleDecoration::DividerPadding() const {
  return kDividerPadding;
}

CGFloat BubbleDecoration::GetWidthForImageAndLabel(NSImage* image,
                                                   NSString* label) {
  if (!image && !label)
    return kOmittedWidth;

  const CGFloat image_width = image ? [image size].width : 0.0;
  if (!label)
    return kBubblePadding + image_width;

  // The bubble needs to take up an integral number of pixels.
  // Generally -sizeWithAttributes: seems to overestimate rather than
  // underestimate, so floor() seems to work better.
  const CGFloat label_width =
      std::floor([label sizeWithAttributes:attributes_].width);
  return kBubblePadding + image_width + kIconLabelPadding + label_width +
         DividerPadding() + kLeftSidePadding;
}

NSRect BubbleDecoration::GetImageRectInFrame(NSRect frame) {
  NSRect image_rect = NSInsetRect(frame, 0.0, kImageFrameYInset);
  if (image_) {
    // Center the image vertically.
    const NSSize image_size = [image_ size];

    image_rect.origin.y +=
        std::floor((NSHeight(image_rect) - image_size.height) / 2.0);
    image_rect.origin.x += kLeftSidePadding;
    image_rect.size = image_size;
    if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout()) {
      image_rect.origin.x =
          NSMaxX(frame) - NSWidth(image_rect) - kLeftSidePadding;
    }
  }
  return image_rect;
}

NSColor* BubbleDecoration::GetDarkModeTextColor() {
  return skia::SkColorToSRGBNSColor(kMaterialDarkModeTextColor);
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

NSRect BubbleDecoration::GetBackgroundFrame(NSRect frame) {
  NSRect background_frame = NSInsetRect(frame, 0.0, kBackgroundFrameYInset);
  if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout()) {
    background_frame.origin.x += kDividerPadding;
    background_frame.size.width -= kDividerPadding + kBackgroundFrameLeftMargin;
  } else {
    background_frame.origin.x += kBackgroundFrameLeftMargin;
    background_frame.size.width -= kDividerPadding;
  }
  return background_frame;
}

void BubbleDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  const NSRect decoration_frame = NSInsetRect(frame, 0.0, kImageFrameYInset);
  CGFloat text_left_offset = NSMinX(decoration_frame);
  CGFloat text_right_offset = NSMaxX(decoration_frame);
  const BOOL is_rtl = cocoa_l10n_util::ShouldDoExperimentalRTLLayout();

  if (image_) {
    NSRect image_rect = GetImageRectInFrame(frame);
    [image_ drawInRect:image_rect
              fromRect:NSZeroRect  // Entire image
             operation:NSCompositeSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
    if (is_rtl)
      text_right_offset = NSMinX(image_rect) - kIconLabelPadding;
    else
      text_left_offset = NSMaxX(image_rect) + kIconLabelPadding;
  }

  // Draw the divider and set the text color.
  NSBezierPath* line = [NSBezierPath bezierPath];
  const CGFloat divider_x_position =
      is_rtl ? NSMinX(decoration_frame) + DividerPadding()
             : NSMaxX(decoration_frame) - DividerPadding();

  [line setLineWidth:1];
  [line moveToPoint:NSMakePoint(divider_x_position, NSMinY(decoration_frame))];
  [line lineToPoint:NSMakePoint(divider_x_position, NSMaxY(decoration_frame))];

  bool in_dark_mode = [[control_view window] inIncognitoModeWithSystemTheme];
  [GetDividerColor(in_dark_mode) set];
  [line stroke];

  NSColor* text_color =
      in_dark_mode ? GetDarkModeTextColor() : GetBackgroundBorderColor();
  SetTextColor(text_color);

  if (label_) {
    NSRect text_rect = frame;
    text_rect.origin.x = text_left_offset;
    text_rect.size.width = text_right_offset - text_left_offset;
    // Transform the coordinate system to adjust the baseline on Retina. This is
    // the only way to get fractional adjustments.
    gfx::ScopedNSGraphicsContextSaveGState saveGraphicsState;
    CGFloat line_width = [control_view cr_lineWidth];
    if (line_width < 1) {
      NSAffineTransform* transform = [NSAffineTransform transform];
      [transform translateXBy:0 yBy:retina_baseline_offset_];
      [transform concat];
    }
    DrawLabel(label_, attributes_, text_rect);
  }
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

void BubbleDecoration::SetRetinaBaselineOffset(CGFloat offset) {
  retina_baseline_offset_ = offset;
}
