// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_

#include "base/callback.h"
#include "chrome/browser/ui/app_list/app_list.h"
#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "ui/app_list/views/app_list_view_observer.h"

namespace app_list {
class AppListView;
}

namespace gfx {
class Display;
class Point;
class Size;
}  // namespace gfx

// Responsible for positioning, hiding and showing an AppListView on Linux.
// This includes watching window activation/deactivation messages to determine
// if the user has clicked away from it.
class AppListLinux : public AppList,
                     public app_list::AppListViewObserver {
 public:
  AppListLinux(app_list::AppListView* view,
               const base::Closure& on_should_dismiss);
  virtual ~AppListLinux();

  // Finds the position for a window to anchor it to the shelf. This chooses the
  // most appropriate position for the window based on whether the shelf exists,
  // the position of the shelf, and the mouse cursor. Returns the intended
  // coordinates for the center of the window. If |shelf_rect| is empty, assumes
  // there is no shelf on the given display.
  static gfx::Point FindAnchorPoint(const gfx::Size& view_size,
                                    const gfx::Display& display,
                                    const gfx::Point& cursor,
                                    AppListPositioner::ScreenEdge edge);

  // AppList:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void MoveNearCursor() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual void Prerender() OVERRIDE;
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
