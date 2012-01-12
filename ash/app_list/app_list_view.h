// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_H_
#define ASH_APP_LIST_APP_LIST_VIEW_H_
#pragma once

#include "ash/app_list/app_list_item_view_listener.h"
#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class View;
}

namespace ash {

class AppListModel;
class AppListViewDelegate;

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppListModelView and passes AppListModel to it for display.
class ASH_EXPORT AppListView : public views::WidgetDelegateView,
                                      public AppListItemViewListener {
 public:
  // Takes ownership of |model| and |delegate|.
  AppListView(AppListModel* model,
              AppListViewDelegate* delegate,
              const gfx::Rect& bounds);
  virtual ~AppListView();

  // Closes app list.
  void Close();

 private:
  // Initializes the window.
  void Init(const gfx::Rect& bounds);

  // Overridden from views::View:
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  // Overridden from AppListItemModelViewListener:
  virtual void AppListItemActivated(AppListItemView* sender,
                                    int event_flags) OVERRIDE;

  scoped_ptr<AppListModel> model_;

  scoped_ptr<AppListViewDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_VIEW_H_
