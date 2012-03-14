// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_H_
#define ASH_APP_LIST_APP_LIST_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class View;
}

namespace ash {

class AppListModel;
class AppListModelView;
class AppListViewDelegate;

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppListModelView and passes AppListModel to it for display.
class AppListView : public views::WidgetDelegateView,
                    public views::ButtonListener {
 public:
  // Takes ownership of |delegate|.
  AppListView(AppListViewDelegate* delegate,
              const gfx::Rect& bounds);
  virtual ~AppListView();

  void AnimateShow();
  void AnimateHide();

  void Close();

 private:
  // Initializes the window.
  void Init(const gfx::Rect& bounds);

  // Updates model using query text in search box.
  void UpdateModel();

  // Overridden from views::WidgetDelegateView:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  scoped_ptr<AppListModel> model_;

  scoped_ptr<AppListViewDelegate> delegate_;

  AppListModelView* model_view_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_VIEW_H_
