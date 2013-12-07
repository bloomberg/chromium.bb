// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_

#include "base/callback.h"
#include "chrome/browser/ui/app_list/app_list.h"
#include "ui/app_list/views/app_list_view_observer.h"

namespace app_list {
class AppListView;
}

// Responsible for positioning, hiding and showing an AppListView on Linux.
// This includes watching window activation/deactivation messages to determine
// if the user has clicked away from it.
class AppListLinux : public AppList,
                     public app_list::AppListViewObserver {
 public:
  AppListLinux(app_list::AppListView* view,
               const base::Closure& on_should_dismiss);
  virtual ~AppListLinux();

  // AppList:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void MoveNearCursor() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual void Prerender() OVERRIDE;
  virtual void ReactivateOnNextFocusLoss() OVERRIDE;
  virtual gfx::NativeWindow GetWindow() OVERRIDE;
  virtual void SetProfile(Profile* profile) OVERRIDE;

  // app_list::AppListViewObserver:
  virtual void OnActivationChanged(views::Widget* widget, bool active) OVERRIDE;

 private:
  // Weak pointer. The view manages its own lifetime.
  app_list::AppListView* view_;
  bool window_icon_updated_;

  // Called to request |view_| be closed.
  base::Closure on_should_dismiss_;

  DISALLOW_COPY_AND_ASSIGN(AppListLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_
