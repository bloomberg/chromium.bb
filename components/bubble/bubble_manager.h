// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BUBBLE_BUBBLE_MANAGER_H_
#define COMPONENTS_BUBBLE_BUBBLE_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/bubble/bubble_close_reason.h"

class BubbleController;
class BubbleDelegate;

typedef base::WeakPtr<BubbleController> BubbleReference;

// Inherit from BubbleManager to show, update, and close bubbles.
// Any class that inherits from BubbleManager should capture any events that
// should dismiss a bubble or update its anchor point.
// This class assumes that we won't be showing a lot of bubbles simultaneously.
// TODO(hcarmona): Handle simultaneous bubbles. http://crbug.com/366937
class BubbleManager {
 public:
  // Should be instantiated on the UI thread.
  BubbleManager();
  virtual ~BubbleManager();

  // Shows a specific bubble and returns a reference to it.
  // This reference should be used through the BubbleManager.
  BubbleReference ShowBubble(scoped_ptr<BubbleDelegate> bubble);

  // Notify a bubble of an event that might trigger close.
  // Returns true if the bubble was actually closed.
  bool CloseBubble(BubbleReference bubble, BubbleCloseReason reason);

  // Notify all bubbles of an event that might trigger close.
  void CloseAllBubbles(BubbleCloseReason reason);

  // Notify all bubbles that their anchor or parent may have changed.
  void UpdateAllBubbleAnchors();

 private:
  enum ManagerStates {
    SHOW_BUBBLES,
    QUEUE_BUBBLES,
    NO_MORE_BUBBLES,
  };

  // Show any bubbles that were added to |show_queue_|.
  void ShowPendingBubbles();

  // Verify that functions that affect the UI are done on the same thread.
  base::ThreadChecker thread_checker_;

  // Determines what happens to a bubble when |ShowBubble| is called.
  ManagerStates manager_state_;

  // The bubbles that are being managed.
  ScopedVector<BubbleController> controllers_;

  // The bubbles queued to be shown when possible.
  ScopedVector<BubbleController> show_queue_;

  DISALLOW_COPY_AND_ASSIGN(BubbleManager);
};

#endif  // COMPONENTS_BUBBLE_BUBBLE_MANAGER_H_
