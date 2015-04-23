// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GESTURE_MANAGER_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_GESTURE_MANAGER_DELEGATE_H_

#include <set>

#include "components/view_manager/public/cpp/types.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace view_manager {

class GestureManagerDelegate {
 public:
  // Informs the delegate of a pointer event. The delegate should asynchronously
  // respond with the set of gestures appropriate for the view
  // (GestureManager::SetGestures()).
  // |has_chosen_gesture| is true if a gesture has been chosen.
  virtual void ProcessEvent(const ServerView* view,
                            mojo::EventPtr event,
                            bool has_chosen_gesture) = 0;

 protected:
  virtual ~GestureManagerDelegate() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_GESTURE_MANAGER_DELEGATE_H_
