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

  void RegainNextLostFocus() {
    regain_next_lost_focus_ = true;
  }

  // app_list::AppListView::Observer overrides.
  virtual void OnActivationChanged(views::Widget* widget, bool active) OVERRIDE;

  void OnViewHidden();

 private:
  void CheckTaskbarOrViewHasFocus();

  // The window to track the active state of.
  app_list::AppListView* view_;

  // Called to request |view_| be closed.
  base::Closure on_should_dismiss_;

  // True if we are anticipating that the app list will lose focus, and we want
  // to take it back. This is used when switching out of Metro mode, and the
  // browser regains focus after showing the app list.
  bool regain_next_lost_focus_;

  // When the context menu on the app list's taskbar icon is brought up the
  // app list should not be hidden, but it should be if the taskbar is clicked
  // on. There can be a period of time when the taskbar gets focus between a
  // right mouse click and the menu showing; to prevent hiding the app launcher
  // when this happens it is kept visible if the taskbar is seen briefly without
  // the right mouse button down, but not if this happens twice in a row.
  bool preserving_focus_for_taskbar_menu_;

  // Timer used to check if the taskbar or app list is active. Using a timer
  // means we don't need to hook Windows, which is apparently not possible
  // since Vista (and is not nice at any time).
  base::RepeatingTimer<ActivationTrackerWin> timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_
