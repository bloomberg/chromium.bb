// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_button.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ManualFillCellButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  auto animations = ^{
    CGFloat alpha = highlighted ? 0.07 : 0;
    self.backgroundColor = [UIColor colorWithWhite:0 alpha:alpha];
  };
  [UIView animateWithDuration:0.1
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:animations
                   completion:nil];
}

@end
