// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/bottom_home_view.h"

#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace athena {

BottomHomeView::BottomHomeView(app_list::AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));
  SetBorder(views::Border::CreateSolidBorder(1, SK_ColorBLACK));
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0));

  app_list::AppListModel* model = view_delegate->GetModel();
  app_list::AppListItemList* top_level = model->top_level_item_list();

  views::View* items_container = new views::View();
  AddChildView(items_container);

  views::BoxLayout* items_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0);
  items_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_FILL);
  items_container->SetLayoutManager(items_layout);
  for (size_t i = 0; i < top_level->item_count(); ++i) {
    app_list::TileItemView* tile_item_view = new app_list::TileItemView();
    tile_item_view->SetAppListItem(top_level->item_at(i));
    items_container->AddChildView(tile_item_view);
  }

  app_list::SearchBoxView* search_box = new app_list::SearchBoxView(
      this, view_delegate);
  AddChildView(search_box);
}

BottomHomeView::~BottomHomeView() {}

void BottomHomeView::QueryChanged(app_list::SearchBoxView* sender) {
  // Nothing needs to be done.
}

}  // namespace athena
