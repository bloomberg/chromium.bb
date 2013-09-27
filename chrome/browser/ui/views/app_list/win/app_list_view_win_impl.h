// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_VIEW_WIN_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_VIEW_WIN_IMPL_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/views/app_list/win/activation_tracker.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_win.h"
#include "ui/app_list/views/app_list_view.h"

// Responsible for positioning, hiding and showing an AppListView on Windows.
// This includes watching window activation/deactivation messages to determine
// if the user has clicked away from it.
class AppListViewWinImpl : public AppListViewWin {
 public:
  AppListViewWinImpl(app_list::AppListView* view,
                     const base::Closure& on_should_dismiss);
  virtual ~AppListViewWinImpl();

  // AppListViewWin overrides.
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void MoveNearCursor() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual void Prerender() OVERRIDE;
  virtual void RegainNextLostFocus() OVERRIDE;
  virtual gfx::NativeWindow GetWindow() OVERRIDE;
  virtual void SetProfile(Profile* profile) OVERRIDE;

  app_list::AppListModel* model() {
    return view_->model();
  }

 private:
  // Weak pointer. The view manages its own lifetime.
  app_list::AppListView* view_;
  ActivationTracker activation_tracker_;
  bool window_icon_updated_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewWinImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_VIEW_WIN_IMPL_H_
