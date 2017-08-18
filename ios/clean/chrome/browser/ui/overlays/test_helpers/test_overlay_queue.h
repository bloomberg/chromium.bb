// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_QUEUE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_QUEUE_H_

#import "ios/clean/chrome/browser/ui/overlays/overlay_queue.h"

@class BrowserCoordinator;
@class OverlayCoordinator;

// Test OverlayQueue implementation.  This object constructs a dummy parent
// coordinator from which to start queued OverlayCoordinators.
class TestOverlayQueue : public OverlayQueue {
 public:
  TestOverlayQueue();
  void StartNextOverlay() override;
  void AddOverlay(OverlayCoordinator* overlay);

 private:
  // The coordinator to use as the parent for overlays added via AddOverlay().
  __strong BrowserCoordinator* parent_;

  DISALLOW_COPY_AND_ASSIGN(TestOverlayQueue);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_QUEUE_H_
