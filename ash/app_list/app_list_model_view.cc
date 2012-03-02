// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_view.h"

#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"
#include "base/utf_string_conversions.h"

namespace ash {

namespace {

// Minimum label width.
const int kMinLabelWidth = 150;

// Calculate preferred tile size for given |content_size| and |num_of_tiles|.
gfx::Size CalculateTileSize(const gfx::Size& content_size, int num_of_tiles) {
  // Icon sizes to try.
  const int kIconSizes[] = { 64, 48, 32, 16 };

  int tile_height = 0;
  int tile_width = 0;
  int rows = 0;
  int cols = 0;

  // Chooses the biggest icon size that could fit all tiles.
  for (size_t i = 0; i < arraysize(kIconSizes); ++i) {
    int icon_size = kIconSizes[i];
    tile_height = icon_size + 2 * AppListItemView::kPadding;
    tile_width = icon_size + std::min(kMinLabelWidth, icon_size * 2) +
        2 * AppListItemView::kPadding;

    rows = content_size.height() / tile_height;
    cols = std::min(content_size.width() / tile_width,
                    (num_of_tiles - 1) / rows + 1);
    if (rows * cols >= num_of_tiles)
      break;
  }

  if (rows && cols) {
    // Adjusts tile width to fit |content_size| as much as possible.
    tile_width = std::max(tile_width, content_size.width() / cols);
    return gfx::Size(tile_width, tile_height);
  }

  return gfx::Size();
}

}  // namespace

AppListModelView::AppListModelView(views::ButtonListener* listener)
    : model_(NULL),
      listener_(listener) {
}

AppListModelView::~AppListModelView() {
  if (model_)
    model_->RemoveObserver(this);
}

void AppListModelView::SetModel(AppListModel* model) {
  DCHECK(model);

  if (model_)
    model_->RemoveObserver(this);

  model_ = model;
  model_->AddObserver(this);
  Update();
}

void AppListModelView::Update() {
  RemoveAllChildViews(true);
  if (!model_ || model_->item_count() == 0)
    return;

  for (int i = 0; i < model_->item_count(); ++i)
    AddChildView(new AppListItemView(model_->GetItem(i), listener_));

  Layout();
  SchedulePaint();
}

void AppListModelView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Size tile_size = CalculateTileSize(rect.size(), child_count());

  int col_bottom = rect.bottom();
  gfx::Rect current_tile(rect.origin(), tile_size);

  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    view->SetBoundsRect(current_tile);

    current_tile.Offset(0, tile_size.height());
    if (current_tile.bottom() >= col_bottom) {
      current_tile.set_x(current_tile.x() + tile_size.width());
      current_tile.set_y(rect.y());
    }
  }
}

void AppListModelView::ListItemsAdded(int start, int count) {
  Update();
}

void AppListModelView::ListItemsRemoved(int start, int count) {
  Update();
}

void AppListModelView::ListItemsChanged(int start, int count) {
  NOTREACHED();
}

}  // namespace ash
