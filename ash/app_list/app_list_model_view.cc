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

    rows = std::max(content_size.height() / tile_height, 1);
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
      listener_(listener),
      selected_item_index_(-1),
      items_per_col_(0) {
  set_focusable(true);
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

int AppListModelView::SetTileIconSizeAndGetMaxWidth(int icon_dimension) {
  gfx::Size icon_size(icon_dimension, icon_dimension);
  int max_tile_width = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    static_cast<AppListItemView*>(view)->set_icon_size(icon_size);
    gfx::Size preferred_size = view->GetPreferredSize();
    if (preferred_size.width() > max_tile_width)
      max_tile_width = preferred_size.width();
  }

  return max_tile_width;
}

void AppListModelView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    items_per_col_ = 0;
    return;
  }

  // Gets |tile_size| based on content rect and number of tiles.
  gfx::Size tile_size = CalculateTileSize(rect.size(), child_count());
  items_per_col_ = rect.height() / tile_size.height();

  // Sets tile's icons size and caps tile width to the max tile width.
  int max_tile_width = SetTileIconSizeAndGetMaxWidth(
      tile_size.height() - 2 * AppListItemView::kPadding);
  if (max_tile_width && tile_size.width() > max_tile_width)
    tile_size.set_width(max_tile_width);

  // Layouts tiles.
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

bool AppListModelView::OnKeyPressed(const views::KeyEvent& event) {
  bool handled = false;
  if (selected_item_index_ >= 0)
    handled = GetItemViewAtIndex(selected_item_index_)->OnKeyPressed(event);

  if (!handled) {
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        SetSelectedItemByIndex(std::max(selected_item_index_ - items_per_col_,
                                        0));
        return true;
      case ui::VKEY_RIGHT:
        if (selected_item_index_ < 0) {
          SetSelectedItemByIndex(0);
        } else {
          SetSelectedItemByIndex(std::min(selected_item_index_ + items_per_col_,
                                          child_count() - 1));
        }
        return true;
      case ui::VKEY_UP:
        SetSelectedItemByIndex(std::max(selected_item_index_ - 1, 0));
        return true;
      case ui::VKEY_DOWN:
        SetSelectedItemByIndex(std::min(selected_item_index_ + 1,
                                        child_count() - 1));
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
  Update();
}

void AppListModelView::ListItemsRemoved(int start, int count) {
  Update();
}

void AppListModelView::ListItemsChanged(int start, int count) {
  NOTREACHED();
}

}  // namespace ash
