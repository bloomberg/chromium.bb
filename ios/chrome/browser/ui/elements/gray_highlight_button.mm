// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/gray_highlight_button.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GrayHighlightButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted) {
    self.backgroundColor = [UIColor colorWithWhite:235.0 / 255.0 alpha:1.0];
  } else {
    self.backgroundColor = [UIColor clearColor];
  }
}

@end
