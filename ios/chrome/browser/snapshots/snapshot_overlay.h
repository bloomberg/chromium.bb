// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_OVERLAY_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_OVERLAY_H_

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"

// Simple object containing all the information needed to display an overlay
// view in a snapshot.
@interface SnapshotOverlay : NSObject {
 @private
  // The overlay view.
  base::scoped_nsobject<UIView> view_;

  // Y offset for the overlay view. Used to adjust the y position of |view_|
  // within the snapshot.
  CGFloat yOffset_;
}

// Initialize SnapshotOverlay with the given view and yOffset.
- (id)initWithView:(UIView*)view yOffset:(CGFloat)yOffset;
- (UIView*)view;
- (CGFloat)yOffset;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_OVERLAY_H_
