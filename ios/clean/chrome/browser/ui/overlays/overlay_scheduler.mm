// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/overlays/overlay_scheduler.h"

#include <list>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_queue.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_queue_manager.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_scheduler_observer.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_BROWSER_USER_DATA_KEY(OverlayScheduler);

OverlayScheduler::OverlayScheduler(Browser* browser) : queue_manager_(nullptr) {
  // Create the OverlayQueueManager and add the scheduler as an observer to the
  // manager and all its queues.
  OverlayQueueManager::CreateForBrowser(browser);
  queue_manager_ = OverlayQueueManager::FromBrowser(browser);
  queue_manager_->AddObserver(this);
  for (OverlayQueue* queue : queue_manager_->queues()) {
    queue->AddObserver(this);
  }
}

OverlayScheduler::~OverlayScheduler() {
  // Disconnect() removes the scheduler as an observer and nils out
  // |queue_manager_|.  It is expected to be called before deallocation.
  DCHECK(!queue_manager_);
}

#pragma mark - Public

void OverlayScheduler::AddObserver(OverlaySchedulerObserver* observer) {
  observers_.AddObserver(observer);
}

void OverlayScheduler::RemoveObserver(OverlaySchedulerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OverlayScheduler::SetQueueManager(OverlayQueueManager* queue_manager) {
  if (queue_manager_) {
    queue_manager_->RemoveObserver(this);
    for (OverlayQueue* queue : queue_manager_->queues()) {
      StopObservingQueue(queue);
    }
  }
  queue_manager_ = queue_manager;
  if (queue_manager_) {
    queue_manager_->AddObserver(this);
    for (OverlayQueue* queue : queue_manager_->queues()) {
      queue->AddObserver(this);
    }
  }
}

bool OverlayScheduler::IsShowingOverlay() const {
  return !overlay_queues_.empty() &&
         overlay_queues_.front()->IsShowingOverlay();
}

void OverlayScheduler::ReplaceVisibleOverlay(
    OverlayCoordinator* overlay_coordinator) {
  DCHECK(overlay_coordinator);
  DCHECK(IsShowingOverlay());
  overlay_queues_.front()->ReplaceVisibleOverlay(overlay_coordinator);
}

void OverlayScheduler::CancelOverlays() {
  // |overlay_queues_| will be updated in OverlayQueueDidCancelOverlays(), so a
  // while loop is used to avoid using invalidated iterators.
  while (!overlay_queues_.empty()) {
    overlay_queues_.front()->CancelOverlays();
  }
}

void OverlayScheduler::Disconnect() {
  queue_manager_->RemoveObserver(this);
  for (OverlayQueue* queue : queue_manager_->queues()) {
    StopObservingQueue(queue);
  }
  queue_manager_->Disconnect();
  queue_manager_ = nullptr;
}

#pragma mark - OverlayQueueManagerObserver

void OverlayScheduler::OverlayQueueManagerDidAddQueue(
    OverlayQueueManager* manager,
    OverlayQueue* queue) {
  queue->AddObserver(this);
}

void OverlayScheduler::OverlayQueueManagerWillRemoveQueue(
    OverlayQueueManager* manager,
    OverlayQueue* queue) {
  StopObservingQueue(queue);
}

#pragma mark - OverlayQueueObserver

void OverlayScheduler::OverlayQueueDidAddOverlay(OverlayQueue* queue) {
  DCHECK(queue);
  overlay_queues_.push_back(queue);
  TryToStartNextOverlay();
}

void OverlayScheduler::OverlayQueueWillReplaceVisibleOverlay(
    OverlayQueue* queue) {
  DCHECK(queue);
  DCHECK_EQ(overlay_queues_.front(), queue);
  DCHECK(queue->IsShowingOverlay());
  // An OverlayQueue's visible overlay can only be replaced if it's the first
  // queue in the scheduler and is already showing an overlay.  The queue is
  // added here to schedule its replacement overlay can be displayed when its
  // currently-visible overlay is stopped.
  overlay_queues_.push_front(queue);
}

void OverlayScheduler::OverlayQueueDidStopVisibleOverlay(OverlayQueue* queue) {
  DCHECK(!overlay_queues_.empty());
  DCHECK_EQ(overlay_queues_.front(), queue);
  // Only the first queue in the scheduler can start overlays, so it is expected
  // that this function is only called for that queue.
  overlay_queues_.pop_front();
  TryToStartNextOverlay();
}

void OverlayScheduler::OverlayQueueDidCancelOverlays(OverlayQueue* queue) {
  DCHECK(queue);
  // Remove all scheduled instances of |queue| from the |overlay_queues_|.
  auto i = overlay_queues_.begin();
  while (i != overlay_queues_.end()) {
    if (*i == queue)
      overlay_queues_.erase(i);
  }
  // If |queue| is currently showing an overlay, prepend it to
  // |overlay_queues_|.  It will be removed when the cancelled overlay is
  // stopped.
  if (queue->IsShowingOverlay())
    overlay_queues_.push_front(queue);
}

#pragma mark -

void OverlayScheduler::TryToStartNextOverlay() {
  // Early return if an overlay is already started or if there are no queued
  // overlays to show.
  if (overlay_queues_.empty() || IsShowingOverlay())
    return;
  // Notify the observers that the next overlay is about to be shown.
  OverlayQueue* queue = overlay_queues_.front();
  web::WebState* web_state = queue->GetWebState();
  for (auto& observer : observers_) {
    observer.OverlaySchedulerWillShowOverlay(this, web_state);
  }
  // Start the next overlay in the first queue.
  queue->StartNextOverlay();
}

void OverlayScheduler::StopObservingQueue(OverlayQueue* queue) {
  queue->RemoveObserver(this);
  queue->CancelOverlays();
}
