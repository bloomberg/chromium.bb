// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/bottom_home_view.h"

#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/shadow_border.h"

namespace {

class BottomHomeBackground : public views::Background {
 public:
  explicit BottomHomeBackground(views::View* search_box)
      : search_box_(search_box),
        painter_(views::Painter::CreateVerticalGradient(
            SkColorSetA(SK_ColorWHITE, 0x7f),
            SK_ColorWHITE)) {}
  virtual ~BottomHomeBackground() {}

 private:
  // views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    CHECK_EQ(view, search_box_->parent());
    views::Painter::PaintPainterAt(
        canvas,
        painter_.get(),
        gfx::Rect(0, 0, view->width(), search_box_->y()));
    canvas->FillRect(gfx::Rect(0,
                               search_box_->y(),
                               view->width(),
                               view->height() - search_box_->y()),
                     SK_ColorWHITE);
  }

  views::View* search_box_;
  scoped_ptr<views::Painter> painter_;
  DISALLOW_COPY_AND_ASSIGN(BottomHomeBackground);
};

}  // namespace

namespace athena {

BottomHomeView::BottomHomeView(app_list::AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  app_list::AppListModel* model = view_delegate->GetModel();
  app_list::AppListItemList* top_level = model->top_level_item_list();

  views::View* items_container = new views::View();
  AddChildView(items_container);

  views::BoxLayout* items_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0);
  items_layout->SetDefaultFlex(1);
  items_container->SetLayoutManager(items_layout);
  for (size_t i = 0; i < top_level->item_count(); ++i) {
    app_list::TileItemView* tile_item_view = new app_list::TileItemView();
    tile_item_view->SetAppListItem(top_level->item_at(i));
    items_container->AddChildView(tile_item_view);
    tile_item_view->set_background(NULL);
  }

  app_list::SearchBoxView* search_box = new app_list::SearchBoxView(
      this, view_delegate);
  AddChildView(search_box);
  search_box->SetBorder(
      views::Border::CreateSolidSidedBorder(1, 0, 0, 0, SK_ColorGRAY));

  set_background(new BottomHomeBackground(search_box));
}

BottomHomeView::~BottomHomeView() {}

void BottomHomeView::QueryChanged(app_list::SearchBoxView* sender) {
  // Nothing needs to be done.
}

}  // namespace athena
