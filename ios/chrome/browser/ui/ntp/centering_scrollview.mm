// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/centering_scrollview.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/ui_util.h"

namespace {
// Thickness of scrollbar images inserted by UIScrollView (circa iOS 5.0).
const CGFloat kScrollBarThickness = 7.0;
}  // namespace

@interface CenteringScrollView ()
// Returns the one and only subview. There may be other subviews
// (e.g. UIImageView for the scrollbars) created by UIScrollView.
- (UIView*)findContentView;
@end

@implementation CenteringScrollView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Find the movable content view inside this scroll view.
  UIView* contentView = [self findContentView];
  // UIScrollView does not send |layoutSubviews| message to its subviews.
  // Since layoutSubviews may change the frame of |contentView|, this must be
  // called before the centering computation below.
  [contentView layoutSubviews];

  // Place content view in the center of frame.
  CGRect frame = self.frame;
  CGRect contentRect = contentView.frame;
  contentRect.origin = AlignPointToPixel(CGPointMake(
      std::max((frame.size.width - contentRect.size.width) / 2.0, 0.0),
      std::max((frame.size.height - contentRect.size.height) / 2.0, 0.0)));
  contentView.frame = contentRect;

  if (contentRect.size.width > frame.size.width ||
      contentRect.size.height > frame.size.height) {
    // If size of content view is larger than frame size, adjust content size
    // so it becomes scrollable.
    self.contentSize =
        CGSizeMake(std::max(contentRect.size.width, frame.size.width),
                   std::max(contentRect.size.height, frame.size.height));
  } else {
    // Reset the content size as the view need not be scollable.
    self.contentSize = CGSizeMake(0.0, 0.0);
  }
}

#pragma mark -
#pragma mark Private

- (UIView*)findContentView {
  UIView* soloView = nil;
  NSUInteger viewCount = 0;
  for (UIView* view in [self subviews]) {
    // Ignore any UIImageView that has a smaller dimension less than the
    // thickness of a scroll bar. This detection heuristics is based on the
    // scrollbar sizing of iOS UIScrollView circa iOS 5.0. If this changes,
    // the DCHECK following this for-loop will likely fail.
    CGFloat thickness = std::min(view.frame.size.width, view.frame.size.height);
    if ([view isKindOfClass:[UIImageView class]] &&
        thickness <= kScrollBarThickness)
      continue;
    soloView = view;
    ++viewCount;
  }
  DCHECK(viewCount == 1);
  return soloView;
}

@end
