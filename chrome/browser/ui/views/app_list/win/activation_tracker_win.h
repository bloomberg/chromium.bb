// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/app_list/views/app_list_view_observer.h"

class AppListServiceWin;

// Periodically checks to see if an AppListView has lost focus using a timer.
class ActivationTrackerWin : public app_list::AppListViewObserver {
 public:
  explicit ActivationTrackerWin(AppListServiceWin* service);
  ~ActivationTrackerWin();

  // app_list::AppListViewObserver:
  virtual void OnActivationChanged(views::Widget* widget, bool active) OVERRIDE;

  void OnViewHidden();

 private:
  // Dismisses the app launcher if it has lost focus and the user is not trying
  // to pin it.
  void MaybeDismissAppList();

  // Determines whether the app launcher should be dismissed. This should be
  // called at most once per timer tick, as it is not idempotent (if the taskbar
  // is focused, it waits until it has been called twice before dismissing the
  // app list).
  bool ShouldDismissAppList();

  AppListServiceWin* service_;  // Weak. Owns this.

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

  DISALLOW_COPY_AND_ASSIGN(ActivationTrackerWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_ACTIVATION_TRACKER_WIN_H_
