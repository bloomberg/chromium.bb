// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_queue.h"

#import "ios/clean/chrome/browser/ui/overlays/overlay_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_coordinator.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_parent_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestOverlayQueue::TestOverlayQueue()
    : OverlayQueue(), parent_([[TestOverlayParentCoordinator alloc] init]) {}

void TestOverlayQueue::StartNextOverlay() {
  [GetFirstOverlay() startOverlayingCoordinator:parent_];
  OverlayWasStarted();
}

void TestOverlayQueue::AddOverlay(OverlayCoordinator* overlay) {
  OverlayQueue::AddOverlay(overlay);
}
