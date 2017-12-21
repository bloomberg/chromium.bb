// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/fake_snapshot_generator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeSnapshotGeneratorDelegate

- (UIImage*)defaultSnapshotImage {
  return nil;
}

- (UIEdgeInsets)snapshotEdgeInsets {
  return UIEdgeInsetsZero;
}

- (NSArray<SnapshotOverlay*>*)snapshotOverlays {
  return nil;
}

- (void)willUpdateSnapshotForWebState:(web::WebState*)webState {
}

- (void)didUpdateSnapshotForWebState:(web::WebState*)webState
                           withImage:(UIImage*)snapshot {
}

@end
