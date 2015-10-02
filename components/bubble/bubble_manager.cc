// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_manager.h"

#include <vector>

#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_delegate.h"

BubbleManager::BubbleManager() : manager_state_(SHOW_BUBBLES) {}

BubbleManager::~BubbleManager() {
  FinalizePendingRequests();
}

BubbleReference BubbleManager::ShowBubble(scoped_ptr<BubbleDelegate> bubble) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(bubble);

  scoped_ptr<BubbleController> controller(
      new BubbleController(this, bubble.Pass()));

  BubbleReference bubble_ref = controller->AsWeakPtr();

  switch (manager_state_) {
    case SHOW_BUBBLES:
      controller->Show();
      controllers_.push_back(controller.Pass());
      break;
    case QUEUE_BUBBLES:
      show_queue_.push_back(controller.Pass());
      break;
    case NO_MORE_BUBBLES:
      FOR_EACH_OBSERVER(BubbleManagerObserver, observers_,
                        OnBubbleNeverShown(controller->AsWeakPtr()));
      break;
  }

  return bubble_ref;
}

bool BubbleManager::CloseBubble(BubbleReference bubble,
                                BubbleCloseReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return CloseAllMatchingBubbles(bubble.get(), reason);
}

void BubbleManager::CloseAllBubbles(BubbleCloseReason reason) {
  // The following close reasons don't make sense for multiple bubbles:
  DCHECK_NE(reason, BUBBLE_CLOSE_ACCEPTED);
  DCHECK_NE(reason, BUBBLE_CLOSE_CANCELED);
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseAllMatchingBubbles(nullptr, reason);
}

void BubbleManager::UpdateAllBubbleAnchors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto controller : controllers_)
    controller->UpdateAnchorPosition();
}

void BubbleManager::AddBubbleManagerObserver(BubbleManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BubbleManager::RemoveBubbleManagerObserver(
    BubbleManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BubbleManager::FinalizePendingRequests() {
  // Return if already "Finalized".
  if (manager_state_ == NO_MORE_BUBBLES)
    return;

  manager_state_ = NO_MORE_BUBBLES;
  CloseAllBubbles(BUBBLE_CLOSE_FORCED);
}

void BubbleManager::ShowPendingBubbles() {
  if (manager_state_ == QUEUE_BUBBLES)
    manager_state_ = SHOW_BUBBLES;

  if (manager_state_ == SHOW_BUBBLES) {
    for (auto controller : show_queue_)
      controller->Show();

    controllers_.insert(controllers_.end(), show_queue_.begin(),
                        show_queue_.end());

    show_queue_.weak_clear();
  } else {
    for (auto controller : show_queue_) {
      FOR_EACH_OBSERVER(BubbleManagerObserver, observers_,
                        OnBubbleNeverShown(controller->AsWeakPtr()));
    }

    // Clear the queue if bubbles can't be shown.
    show_queue_.clear();
  }
}

bool BubbleManager::CloseAllMatchingBubbles(BubbleController* match,
                                            BubbleCloseReason reason) {
  ScopedVector<BubbleController> close_queue;

  for (auto iter = controllers_.begin(); iter != controllers_.end();) {
    if ((!match || match == *iter) && (*iter)->ShouldClose(reason)) {
      close_queue.push_back(*iter);
      iter = controllers_.weak_erase(iter);
    } else {
      ++iter;
    }
  }

  for (auto controller : close_queue) {
    controller->DoClose();

    FOR_EACH_OBSERVER(BubbleManagerObserver, observers_,
                      OnBubbleClosed(controller->AsWeakPtr(), reason));
  }

  return !close_queue.empty();
}
