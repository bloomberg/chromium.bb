// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_FOCUS_CONTROLLER_OBSERVER_H_
#define SERVICES_WINDOW_MANAGER_FOCUS_CONTROLLER_OBSERVER_H_

namespace mojo {
class View;
}

namespace window_manager {

class FocusControllerObserver {
 public:
  // Called when |active| gains focus, or there is no active view
  // (|active| is null in this case.).
  virtual void OnActivated(mojo::View* gained_active) = 0;

  // Called when focus moves to |gained_focus|.
  virtual void OnFocused(mojo::View* gained_focus) = 0;

  // Called when during view activation the currently active view is
  // selected for activation. This can happen when a view requested for
  // activation cannot be activated because a system modal view is active.
  virtual void OnAttemptToReactivateView(mojo::View* request_active,
                                         mojo::View* actual_active) {}

 protected:
  virtual ~FocusControllerObserver() {}
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_FOCUS_CONTROLLER_OBSERVER_H_
