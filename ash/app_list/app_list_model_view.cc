// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_view.h"

#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"

namespace ash {

namespace {

// Calculate preferred icon size for given |content_size| and |num_of_tiles|.
gfx::Size CalculateIconSize(const gfx::Size& content_size, int num_of_tiles) {
  // Icon sizes to try.
  const int kIconSizes[] = { 64, 48, 32 };

  // Chooses the biggest icon size that could fit all tiles.
  gfx::Size icon_size;
  for (size_t i = 0; i < arraysize(kIconSizes); ++i) {
    icon_size.SetSize(kIconSizes[i], kIconSizes[i]);
    gfx::Size tile_size = AppListItemView::GetPreferredSizeForIconSize(
        icon_size);

    int cols = std::max(content_size.width() / tile_size.width(), 1);
    int rows = std::min(content_size.height() / tile_size.height(),
                        (num_of_tiles - 1) / cols + 1);
    if (rows * cols >= num_of_tiles)
      break;
  }

  return icon_size;
}

}  // namespace

AppListModelView::AppListModelView(views::ButtonListener* listener)
    : model_(NULL),
      listener_(listener),
      selected_item_index_(-1),
      items_per_row_(0) {
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

void AppListModelView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    items_per_row_ = 0;
    return;
  }

  // Gets |icon_size| based on content rect and number of tiles.
  gfx::Size icon_size = CalculateIconSize(rect.size(), child_count());

  gfx::Size tile_size = AppListItemView::GetPreferredSizeForIconSize(icon_size);
  int cols = std::max(rect.width() / tile_size.width(), 1);
  tile_size.set_width(rect.width() / cols);
  items_per_row_ = rect.width() / tile_size.width();

  // Layouts items.
  int right = rect.right();
  gfx::Rect current_tile(rect.origin(), tile_size);
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    static_cast<AppListItemView*>(view)->set_icon_size(icon_size);
    view->SetBoundsRect(current_tile);
    view->SetVisible(rect.Contains(current_tile));

    current_tile.Offset(tile_size.width(), 0);
    if (current_tile.right() > right) {
      current_tile.set_x(rect.x());
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
        SetSelectedItemByIndex(std::max(selected_item_index_ - items_per_row_,
                                        0));
        return true;
      case ui::VKEY_DOWN:
        if (selected_item_index_ < 0) {
          SetSelectedItemByIndex(0);
        } else {
          SetSelectedItemByIndex(std::min(selected_item_index_ + items_per_row_,
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
  Update();
}

void AppListModelView::ListItemsRemoved(int start, int count) {
  Update();
}

void AppListModelView::ListItemsChanged(int start, int count) {
  NOTREACHED();
}

}  // namespace ash
