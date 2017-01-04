// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_MUS_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_MUS_H_

#include "base/macros.h"
#include "ui/app_list/presenter/app_list_presenter_delegate.h"
#include "ui/views/pointer_watcher.h"

namespace app_list {
class AppListPresenterImpl;
class AppListView;
class AppListViewDelegateFactory;
}  // namespace app_list

// Mus+ash implementation of AppListPresetnerDelegate.
// Responsible for laying out the app list UI as well as dismissing the app list
// on in response to certain events (e.g. on mouse/touch gesture outside of the
// app list bounds).
class AppListPresenterDelegateMus : public app_list::AppListPresenterDelegate,
                                    public views::PointerWatcher {
 public:
  AppListPresenterDelegateMus(
      app_list::AppListPresenterImpl* presenter,
      app_list::AppListViewDelegateFactory* view_delegate_factory);
  ~AppListPresenterDelegateMus() override;

  // app_list::AppListPresenterDelegate:
  app_list::AppListViewDelegate* GetViewDelegate() override;
  void Init(app_list::AppListView* view,
            int64_t display_id,
            int current_apps_page) override;
  void OnShown(int64_t display_id) override;
  void OnDismissed() override;
  void UpdateBounds() override;
  gfx::Vector2d GetVisibilityAnimationOffset(
      aura::Window* root_window) override;

 private:
  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override;

  // Not owned. Pointer is guaranteed to be valid while this object is alive.
  app_list::AppListPresenterImpl* presenter_;

  // Not owned. Pointer is guaranteed to be valid while this object is alive.
  app_list::AppListViewDelegateFactory* view_delegate_factory_;

  // The current AppListView, owned by its widget.
  app_list::AppListView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateMus);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_MUS_H_
