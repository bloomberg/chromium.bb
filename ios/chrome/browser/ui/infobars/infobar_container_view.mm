// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_view.h"

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration for the alpha change animation.
const CGFloat kAlphaChangeAnimationDuration = 0.35;
}  // namespace

@implementation InfoBarContainerView

- (void)layoutSubviews {
  for (UIView<InfoBarViewSizing>* view in self.subviews) {
    [view sizeToFit];
    CGRect frame = view.frame;
    frame.origin.y = CGRectGetHeight(frame) - [view visibleHeight];
    [view setFrame:frame];
  }
}

#pragma mark - InfobarConsumer

- (void)addInfoBar:(InfoBarIOS*)infoBarIOS position:(NSInteger)position {
  DCHECK_LE((NSUInteger)position, [[self subviews] count]);
  CGRect containerBounds = [self bounds];
  infoBarIOS->Layout(containerBounds);
  UIView<InfoBarViewSizing>* view = infoBarIOS->view();
  [view setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                            UIViewAutoresizingFlexibleHeight];
  [self insertSubview:view atIndex:position];
}

- (CGFloat)topmostVisibleInfoBarHeight {
  for (UIView* view in [self.subviews reverseObjectEnumerator]) {
    return [view sizeThatFits:self.frame.size].height;
  }
  return 0;
}

- (void)animateInfoBarContainerToAlpha:(CGFloat)alpha {
  CGFloat oldAlpha = self.alpha;
  if (oldAlpha > 0 && alpha == 0) {
    [self setUserInteractionEnabled:NO];
  }

  [UIView transitionWithView:self
      duration:kAlphaChangeAnimationDuration
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [self setAlpha:alpha];
      }
      completion:^(BOOL) {
        if (oldAlpha == 0 && alpha > 0) {
          [self setUserInteractionEnabled:YES];
        };
      }];
}

- (void)updateLayout {
  [self setNeedsLayout];
}

@end
