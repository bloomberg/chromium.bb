// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_manager.h"

#include <vector>

#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_delegate.h"

BubbleManager::BubbleManager() : manager_state_(SHOW_BUBBLES) {}

BubbleManager::~BubbleManager() {
  manager_state_ = NO_MORE_BUBBLES;
  CloseAllBubbles(BUBBLE_CLOSE_FORCED);
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
      // The controller will be cleaned up and |bubble_ref| will be invalidated.
      // It's important that the controller is created even though it's
      // destroyed immediately because it will collect metrics about the bubble.
      break;
  }

  return bubble_ref;
}

bool BubbleManager::CloseBubble(BubbleReference bubble,
                                BubbleCloseReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(manager_state_, QUEUE_BUBBLES);
  if (manager_state_ == SHOW_BUBBLES)
    manager_state_ = QUEUE_BUBBLES;

  for (auto iter = controllers_.begin(); iter != controllers_.end(); ++iter) {
    if (*iter == bubble.get()) {
      bool closed = (*iter)->ShouldClose(reason);
      if (closed)
        iter = controllers_.erase(iter);
      ShowPendingBubbles();
      return closed;
    }
  }

  // Attempting to close a bubble that is already closed or that this manager
  // doesn't own is a bug.
  NOTREACHED();
  return false;
}

void BubbleManager::CloseAllBubbles(BubbleCloseReason reason) {
  // The following close reasons don't make sense for multiple bubbles:
  DCHECK_NE(reason, BUBBLE_CLOSE_ACCEPTED);
  DCHECK_NE(reason, BUBBLE_CLOSE_CANCELED);

  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(manager_state_, QUEUE_BUBBLES);
  if (manager_state_ == SHOW_BUBBLES)
    manager_state_ = QUEUE_BUBBLES;

  for (auto iter = controllers_.begin(); iter != controllers_.end();)
    iter = (*iter)->ShouldClose(reason) ? controllers_.erase(iter) : iter + 1;

  ShowPendingBubbles();
}

void BubbleManager::UpdateAllBubbleAnchors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto controller : controllers_)
    controller->UpdateAnchorPosition();
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
    // Clear the queue if bubbles can't be shown.
    show_queue_.clear();
  }
}
