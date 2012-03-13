// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_VIEW_H_
#define ASH_APP_LIST_APP_LIST_ITEM_VIEW_H_
#pragma once

#include "ash/app_list/app_list_item_model_observer.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/custom_button.h"

class SkBitmap;

namespace views {
class ImageView;
class Label;
class MenuRunner;
}

namespace ash {

class AppListItemModel;
class AppListModelView;

class AppListItemView : public views::CustomButton,
                        public views::ContextMenuController,
                        public AppListItemModelObserver {
 public:
  AppListItemView(AppListModelView* list_model_view,
                  AppListItemModel* model,
                  views::ButtonListener* listener);
  virtual ~AppListItemView();

  void SetSelected(bool selected);
  bool selected() const {
    return selected_;
  }

  AppListItemModel* model() const {
    return model_;
  }

  void set_icon_size(const gfx::Size& size) {
    icon_size_ = size;
  }

  // Icon padding
  static const int kPadding = 5;

  // Internal class name.
  static const char kViewClassName[];

 private:
  // AppListItemModelObserver overrides:
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemTitleChanged() OVERRIDE;

  // views::View overrides:
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::ContextMenuController overrides:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point) OVERRIDE;

  // views::CustomButton overrides:
  virtual void StateChanged() OVERRIDE;

  AppListItemModel* model_;

  AppListModelView* list_model_view_;
  views::ImageView* icon_;
  views::Label* title_;

  scoped_ptr<views::MenuRunner> context_menu_runner_;

  gfx::Size icon_size_;
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_ITEM_VIEW_H_
