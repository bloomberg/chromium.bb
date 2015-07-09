// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_overlay.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@implementation SnapshotOverlay {
  base::scoped_nsobject<UIView> _view;
}

@synthesize yOffset = _yOffset;

- (instancetype)initWithView:(UIView*)view yOffset:(CGFloat)yOffset {
  self = [super init];
  if (self) {
    DCHECK(view);
    _view.reset([view retain]);
    _yOffset = yOffset;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (UIView*)view {
  return _view;
}

@end
