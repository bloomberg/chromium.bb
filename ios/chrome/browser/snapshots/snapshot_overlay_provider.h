// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_OVERLAY_PROVIDER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_OVERLAY_PROVIDER_H_

#import <UIKit/UIKit.h>

@class Tab;

@protocol SnapshotOverlayProvider<NSObject>

// Returns an array of objects representing views (e.g. infobar, voice search
// button, etc.) currently overlayed. Overlayed views should be ordered so that
// the bottommost view is at the beginning of the array and the topmost view is
// at the end of the array.
- (NSArray*)snapshotOverlaysForTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_OVERLAY_PROVIDER_H_
