// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_VIEW_H_
#define ASH_APP_LIST_APP_LIST_ITEM_VIEW_H_
#pragma once

#include "ash/app_list/app_list_item_model_observer.h"
#include "ash/ash_export.h"
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

class ASH_EXPORT AppListItemView : public views::CustomButton,
                                   public views::ContextMenuController,
                                   public AppListItemModelObserver {
 public:
  AppListItemView(AppListItemModel* model,
                  views::ButtonListener* listener);
  virtual ~AppListItemView();

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

 protected:
  // AppListItemModelObserver overrides:
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemTitleChanged() OVERRIDE;

  // views::View overrides:
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  // views::ContextMenuController overrides:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  AppListItemModel* model_;

  views::ImageView* icon_;
  views::Label* title_;

  scoped_ptr<views::MenuRunner> context_menu_runner_;

  gfx::Size icon_size_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_ITEM_VIEW_H_
