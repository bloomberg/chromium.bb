// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_overlay.h"

#include "base/logging.h"

@implementation SnapshotOverlay

- (id)initWithView:(UIView*)view yOffset:(CGFloat)yOffset {
  self = [super init];
  if (self) {
    DCHECK(view);
    view_.reset([view retain]);
    yOffset_ = yOffset;
  }
  return self;
}

- (UIView*)view {
  return view_;
}

- (CGFloat)yOffset {
  return yOffset_;
}

@end
