// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_QUEUE_MANAGER_OBSERVER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_QUEUE_MANAGER_OBSERVER_H_

#import "ios/clean/chrome/browser/ui/overlays/overlay_queue_manager_observer.h"

#include <list>

class OverlayQueue;
class OverlayQueueManager;

// Observer class for OverlayQueueManager.
class TestOverlayQueueManagerObserver : public OverlayQueueManagerObserver {
 public:
  TestOverlayQueueManagerObserver();
  ~TestOverlayQueueManagerObserver() override;

  // OverlayQueueManagerObserver:
  void OverlayQueueManagerDidAddQueue(OverlayQueueManager* manager,
                                      OverlayQueue* queue) override;
  void OverlayQueueManagerWillRemoveQueue(OverlayQueueManager* manager,
                                          OverlayQueue* queue) override;

  // Flags recording whether the above callbacks have been received.
  bool did_add_called() { return did_add_called_; }
  bool will_remove_called() { return will_remove_called_; }

  // The added queues that were reported via OverlayQueueManagerObserver
  // callbacks.  The queues are present in the same order in which the callbacks
  // were received.
  const std::list<OverlayQueue*>& queues() { return queues_; }

 private:
  bool did_add_called_;
  bool will_remove_called_;
  std::list<OverlayQueue*> queues_;
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_TEST_TEST_OVERLAY_QUEUE_MANAGER_OBSERVER_H_
