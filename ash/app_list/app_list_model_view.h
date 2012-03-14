// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_MODEL_VIEW_H_
#define ASH_APP_LIST_APP_LIST_MODEL_VIEW_H_
#pragma once

#include "ui/base/models/list_model_observer.h"
#include "ui/views/view.h"

namespace views {
class ButtonListener;
}

namespace ash {

class AppListItemView;
class AppListModel;

// AppListModelView displays the UI for an AppListModel.
class AppListModelView : public views::View,
                         public ui::ListModelObserver {
 public:
  explicit AppListModelView(views::ButtonListener* listener);
  virtual ~AppListModelView();

  // Sets |model| to use. Note this does not take ownership of |model|.
  void SetModel(AppListModel* model);

  void SetSelectedItem(AppListItemView* item);
  void ClearSelectedItem(AppListItemView* item);

 private:
  // Updates from model.
  void Update();

  AppListItemView* GetItemViewAtIndex(int index);
  void SetSelectedItemByIndex(int index);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& event) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from ListModelObserver:
  virtual void ListItemsAdded(int start, int count) OVERRIDE;
  virtual void ListItemsRemoved(int start, int count) OVERRIDE;
  virtual void ListItemsChanged(int start, int count) OVERRIDE;

  AppListModel* model_;  // Owned by parent AppListView.
  views::ButtonListener* listener_;

  int selected_item_index_;
  int items_per_row_;

  DISALLOW_COPY_AND_ASSIGN(AppListModelView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_MODEL_VIEW_H_
