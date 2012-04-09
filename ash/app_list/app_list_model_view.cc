// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_view.h"

#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"

namespace ash {

AppListModelView::AppListModelView(views::ButtonListener* listener)
    : model_(NULL),
      listener_(listener),
      selected_item_index_(-1),
      cols_(0) {
  set_focusable(true);
}

AppListModelView::~AppListModelView() {
  if (model_)
    model_->RemoveObserver(this);
}

void AppListModelView::CalculateLayout(const gfx::Size& content_size,
                                       int num_of_tiles,
                                       gfx::Size* icon_size,
                                       int* rows,
                                       int* cols) {
  DCHECK(!content_size.IsEmpty() && num_of_tiles);

  // Icon sizes to try.
  const int kIconSizes[] = { 128, 96, 64, 48, 32 };

  double aspect = static_cast<double>(content_size.width()) /
      content_size.height();

  // Chooses the biggest icon size that could fit all tiles.
  gfx::Size tile_size;
  for (size_t i = 0; i < arraysize(kIconSizes); ++i) {
    icon_size->SetSize(kIconSizes[i], kIconSizes[i]);
    tile_size = AppListItemView::GetPreferredSizeForIconSize(
        *icon_size);

    int max_cols = content_size.width() / tile_size.width();
    int max_rows = content_size.height() / tile_size.height();

    // Skip if |tile_size| could not fit into |content_size|.
    if (max_cols * max_rows < num_of_tiles)
      continue;

    // Find a rows/cols pair that has a aspect ratio closest to |aspect|.
    double min_aspect_diff = 1e5;
    for (int c = std::max(max_cols / 2, 1); c <= max_cols; ++c) {
      int r = std::min((num_of_tiles - 1) / c + 1, max_rows);
      if (c * r < num_of_tiles)
        continue;

      double aspect_diff = fabs(static_cast<double>(c) / r - aspect);
      if (aspect_diff < min_aspect_diff) {
        *cols = c;
        *rows = r;
        min_aspect_diff = aspect_diff;
      }
    }

    DCHECK((*rows) * (*cols) >= num_of_tiles);
    return;
  }

  // No icon size that could fit all tiles.
  *cols = std::max(content_size.width() / tile_size.width(), 1);
  *rows = (num_of_tiles - 1) / (*cols) + 1;
}

void AppListModelView::SetModel(AppListModel* model) {
  if (model_)
    model_->RemoveObserver(this);

  model_ = model;
  if (model_)
    model_->AddObserver(this);
  Update();
}

void AppListModelView::SetSelectedItem(AppListItemView* item) {
  int index = GetIndexOf(item);
  if (index >= 0)
    SetSelectedItemByIndex(index);
}

void AppListModelView::ClearSelectedItem(AppListItemView* item) {
  int index = GetIndexOf(item);
  if (index == selected_item_index_)
    SetSelectedItemByIndex(-1);
}

void AppListModelView::Update() {
  selected_item_index_ = -1;
  RemoveAllChildViews(true);
  if (!model_ || model_->item_count() == 0)
    return;

  for (int i = 0; i < model_->item_count(); ++i)
    AddChildView(new AppListItemView(this, model_->GetItem(i), listener_));

  Layout();
  SchedulePaint();
}

AppListItemView* AppListModelView::GetItemViewAtIndex(int index) {
  return static_cast<AppListItemView*>(child_at(index));
}

void AppListModelView::SetSelectedItemByIndex(int index) {
  if (selected_item_index_ == index)
    return;

  if (selected_item_index_ >= 0)
    GetItemViewAtIndex(selected_item_index_)->SetSelected(false);

  if (index < 0 || index >= child_count()) {
    selected_item_index_ = -1;
  } else {
    selected_item_index_ = index;
    GetItemViewAtIndex(selected_item_index_)->SetSelected(true);
  }
}

void AppListModelView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || child_count() == 0) {
    cols_ = 0;
    return;
  }

  gfx::Size icon_size;
  int rows = 0;
  CalculateLayout(rect.size(), child_count(), &icon_size, &rows, &cols_);
  if (rows == 0 || cols_ == 0)
    return;

  gfx::Size tile_size = AppListItemView::GetPreferredSizeForIconSize(icon_size);
  tile_size.set_width(std::max(rect.width() / (cols_ + 1),
                               tile_size.width()));
  tile_size.set_height(std::max(rect.height() / (rows + 1),
                                tile_size.height()));

  gfx::Rect grid_rect = rect.Center(gfx::Size(tile_size.width() * cols_,
                                              tile_size.height() * rows));
  grid_rect = grid_rect.Intersect(rect);

  // Layouts items.
  gfx::Rect current_tile(grid_rect.origin(), tile_size);
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    static_cast<AppListItemView*>(view)->SetIconSize(icon_size);
    view->SetBoundsRect(current_tile);
    view->SetVisible(rect.Contains(current_tile));

    current_tile.Offset(tile_size.width(), 0);
    if ((i + 1) % cols_ == 0) {
      current_tile.set_x(grid_rect.x());
      current_tile.set_y(current_tile.y() + tile_size.height());
    }
  }
}

bool AppListModelView::OnKeyPressed(const views::KeyEvent& event) {
  bool handled = false;
  if (selected_item_index_ >= 0)
    handled = GetItemViewAtIndex(selected_item_index_)->OnKeyPressed(event);

  if (!handled) {
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        SetSelectedItemByIndex(std::max(selected_item_index_ - 1, 0));
        return true;
      case ui::VKEY_RIGHT:
        SetSelectedItemByIndex(std::min(selected_item_index_ + 1,
                                        child_count() - 1));
        return true;
      case ui::VKEY_UP:
        SetSelectedItemByIndex(std::max(selected_item_index_ - cols_,
                                        0));
        return true;
      case ui::VKEY_DOWN:
        if (selected_item_index_ < 0) {
          SetSelectedItemByIndex(0);
        } else {
          SetSelectedItemByIndex(std::min(selected_item_index_ + cols_,
                                          child_count() - 1));
        }
        return true;
      default:
        break;
    }
  }

  return handled;
}

bool AppListModelView::OnKeyReleased(const views::KeyEvent& event) {
  bool handled = false;
  if (selected_item_index_ >= 0)
    handled = GetItemViewAtIndex(selected_item_index_)->OnKeyReleased(event);

  return handled;
}

void AppListModelView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Override to not paint focus frame.
}

void AppListModelView::ListItemsAdded(int start, int count) {
  for (int i = start; i < start + count; ++i) {
    AddChildViewAt(new AppListItemView(this, model_->GetItem(i), listener_),
                   i);
  }
  Layout();
  SchedulePaint();
}

void AppListModelView::ListItemsRemoved(int start, int count) {
  for (int i = 0; i < count; ++i)
    delete child_at(start);

  Layout();
  SchedulePaint();
}

void AppListModelView::ListItemsChanged(int start, int count) {
  NOTREACHED();
}

}  // namespace ash
