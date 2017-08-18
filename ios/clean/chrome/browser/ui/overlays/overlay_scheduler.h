// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_INTERNAL_OVERLAY_SCHEDULER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_INTERNAL_OVERLAY_SCHEDULER_H_

#include <Foundation/Foundation.h>
#include <list>

#include "base/observer_list.h"
#import "ios/chrome/browser/ui/browser_list/browser_user_data.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_queue_manager_observer.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_queue_observer.h"

@class OverlayCoordinator;
class OverlaySchedulerObserver;

namespace web {
class WebState;
}

// An object that manages scheduling when overlays from various OverlayQueues
// can be started.  If an OverlayQueue requires a WebState's content area to be
// visible, this class will update the WebStateLists's active WebState.
class OverlayScheduler : public BrowserUserData<OverlayScheduler>,
                         public OverlayQueueObserver,
                         public OverlayQueueManagerObserver {
 public:
  ~OverlayScheduler() override;

  // Adds and removes OverlaySchedulerObservers.
  void AddObserver(OverlaySchedulerObserver* observer);
  void RemoveObserver(OverlaySchedulerObserver* observer);

  // Getter and setter for the OverlayQueueManager.  Setting the manager to a
  // new value will add the OverlayScheduler as an observer to the manager and
  // all of its associated OverlayQueues.
  OverlayQueueManager* queue_manager() { return queue_manager_; }
  void SetQueueManager(OverlayQueueManager* queue_manager);

  // Whether a scheduled overlay is currently being shown.
  bool IsShowingOverlay() const;

  // Replaces the currently visible overlay with |overlay_coordinator|.
  void ReplaceVisibleOverlay(OverlayCoordinator* overlay_coordinator);

  // Cancels all scheduled overlays and empties overlay queues.
  void CancelOverlays();

  // Tells the OverlayScheduler to disconnect itself as an observer before
  // deallocation.
  void Disconnect();

 private:
  friend class BrowserUserData<OverlayScheduler>;

  // Private constructor used by factory method.
  explicit OverlayScheduler(Browser* browser);

  // OverlayQueueManagerObserver:
  void OverlayQueueManagerDidAddQueue(OverlayQueueManager* manager,
                                      OverlayQueue* queue) override;
  void OverlayQueueManagerWillRemoveQueue(OverlayQueueManager* manager,
                                          OverlayQueue* queue) override;

  // OverlayQueueObserver:
  void OverlayQueueDidAddOverlay(OverlayQueue* queue) override;
  void OverlayQueueWillReplaceVisibleOverlay(OverlayQueue* queue) override;
  void OverlayQueueDidStopVisibleOverlay(OverlayQueue* queue) override;
  void OverlayQueueDidCancelOverlays(OverlayQueue* queue) override;

  // Attempts to show the next queued overlay.
  void TryToStartNextOverlay();
  // Cancels outstanding overlays for |queue| and removes this as an observer.
  void StopObservingQueue(OverlayQueue* queue);

  // The OverlaySchedulerObservers.
  base::ObserverList<OverlaySchedulerObserver> observers_;
  // The OverlayQueueManager responsible for creating queues for the Browser
  // associated with this OverlayScheduler.
  OverlayQueueManager* queue_manager_;
  // The OverlayQueues that have queued overlays.  After an overlay is stopped,
  // the first queue is popped and the first ovelay in the next queue is
  // started.
  std::list<OverlayQueue*> overlay_queues_;

  DISALLOW_COPY_AND_ASSIGN(OverlayScheduler);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_OVERLAYS_INTERNAL_OVERLAY_SCHEDULER_H_
