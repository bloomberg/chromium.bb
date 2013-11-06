// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_

#include "base/callback.h"
#include "base/timer/timer.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/views/widget/widget.h"

// Periodically checks to see if an AppListView has lost focus using a timer.
class ActivationTrackerWin : public app_list::AppListView::Observer {
 public:
  ActivationTrackerWin(app_list::AppListView* view,
                       const base::Closure& on_should_dismiss);
  ~ActivationTrackerWin();

  void ReactivateOnNextFocusLoss() {
    reactivate_on_next_focus_loss_ = true;
  }

  // app_list::AppListView::Observer overrides.
  virtual void OnActivationChanged(views::Widget* widget, bool active) OVERRIDE;

  void OnViewHidden();

 private:
  // Dismisses the app launcher if it has lost focus and the user is not trying
  // to pin it. If it is time to dismiss the app launcher, but
  // ReactivateOnNextFocusLoss has been called, reactivates the app launcher
  // instead of dismissing it.
  void MaybeDismissAppList();

  // Determines whether the app launcher should be dismissed. This should be
  // called at most once per timer tick, as it is not idempotent (if the taskbar
  // is focused, it waits until it has been called twice before dismissing the
  // app list).
  bool ShouldDismissAppList();

  // The window to track the active state of.
  app_list::AppListView* view_;

  // Called to request |view_| be closed.
  base::Closure on_should_dismiss_;

  // True if we are anticipating that the app list will lose focus, and we want
  // to take it back. This is used when switching out of Metro mode, and the
  // browser regains focus after showing the app list.
  bool reactivate_on_next_focus_loss_;

  // Records whether, on the previous timer tick, the taskbar had focus without
  // the right mouse button being down. We allow the taskbar to have focus for
  // one tick before dismissing the app list. This allows the app list to be
  // kept visible if the taskbar is seen briefly without the right mouse button
  // down, but not if this happens for two consecutive ticks.
  bool taskbar_has_focus_;

  // Timer used to check if the taskbar or app list is active. Using a timer
  // means we don't need to hook Windows, which is apparently not possible
  // since Vista (and is not nice at any time).
  base::RepeatingTimer<ActivationTrackerWin> timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_
