// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_model.h"

@implementation FindInPageModel

@synthesize enabled = enabled_;
@synthesize matches = matches_;
@synthesize currentIndex = currentIndex_;
@synthesize currentPoint = currentPoint_;

- (NSString*)text {
  return text_;
}

- (void)setEnabled:(BOOL)enabled {
  enabled_ = enabled;
  matches_ = 0;
  currentIndex_ = 0;
  currentPoint_ = CGPointZero;
}

- (void)updateQuery:(NSString*)query matches:(NSUInteger)matches {
  if (query)
    text_.reset([query copy]);
  matches_ = matches;
  currentIndex_ = 0;
}

- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point {
  currentIndex_ = index;
  currentPoint_ = point;
}

@end
