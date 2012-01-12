// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_view.h"

#include "ash/app_list/app_list_groups_view.h"
#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/shell.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

AppListView::AppListView(
    AppListModel* model,
    AppListViewDelegate* delegate,
    const gfx::Rect& bounds)
    : model_(model),
      delegate_(delegate) {
  Init(bounds);
}

AppListView::~AppListView() {
}

void AppListView::Close() {
  if (GetWidget()->IsVisible())
    Shell::GetInstance()->ToggleAppList();
}

void AppListView::Init(const gfx::Rect& bounds) {
  SetLayoutManager(new views::FillLayout);
  AppListGroupsView* groups_view = new AppListGroupsView(model_.get(), this);
  AddChildView(groups_view);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.bounds = bounds;
  widget_params.delegate = this;
  widget_params.keep_on_top = true;
  widget_params.transparent = true;

  views::Widget* widget = new views::Widget;
  widget->Init(widget_params);
  widget->SetContentsView(this);

  if (groups_view->GetFocusedTile())
    groups_view->GetFocusedTile()->RequestFocus();
}

bool AppListView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    Close();
    return true;
  }

  return false;
}

bool AppListView::OnMousePressed(const views::MouseEvent& event) {
  // If mouse click reaches us, this means user clicks on blank area. So close.
  Close();

  return true;
}

void AppListView::AppListItemActivated(AppListItemView* sender,
                                       int event_flags) {
  if (delegate_.get())
    delegate_->OnAppListItemActivated(sender->model(), event_flags);
  Close();
}

}  // namespace ash
