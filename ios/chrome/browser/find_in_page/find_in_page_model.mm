// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_model.h"

#include "base/mac/scoped_nsobject.h"

@implementation FindInPageModel {
  base::scoped_nsobject<NSString> _text;
}

@synthesize enabled = _enabled;
@synthesize matches = _matches;
@synthesize currentIndex = _currentIndex;
@synthesize currentPoint = _currentPoint;

- (NSString*)text {
  return _text;
}

- (void)setEnabled:(BOOL)enabled {
  _enabled = enabled;
  _matches = 0;
  _currentIndex = 0;
  _currentPoint = CGPointZero;
}

- (void)updateQuery:(NSString*)query matches:(NSUInteger)matches {
  if (query)
    _text.reset([query copy]);
  _matches = matches;
  _currentIndex = 0;
}

- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point {
  _currentIndex = index;
  _currentPoint = point;
}

@end
