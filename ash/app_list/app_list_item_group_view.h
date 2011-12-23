// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_GROUP_VIEW_H_
#define ASH_APP_LIST_APP_LIST_ITEM_GROUP_VIEW_H_
#pragma once

#include "ash/ash_export.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/views/view.h"

namespace aura_shell {

class AppListItemGroupModel;
class AppListItemViewListener;

// AppListItemGroupView displays its children tiles in a grid.
class ASH_EXPORT AppListItemGroupView
    : public views::View,
      public ui::ListModelObserver {
 public:
  AppListItemGroupView(AppListItemGroupModel* model,
                       AppListItemViewListener* listener);
  virtual ~AppListItemGroupView();

  // Sets tiles per row.
  void SetTilesPerRow(int tiles_per_row);

  // Gets currently focused tile.
  views::View* GetFocusedTile();

  // Updates tiles page when a tile gets focus.
  void UpdateFocusedTile(views::View* tile);

 private:
  // Updates from model.
  void Update();

  // Sets focused tile by index.
  void SetFocusedTileByIndex(int index);

  // Overridden from views::View:
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;

  // Overridden from ListModelObserver:
  virtual void ListItemsAdded(int start, int count) OVERRIDE;
  virtual void ListItemsRemoved(int start, int count) OVERRIDE;
  virtual void ListItemsChanged(int start, int count) OVERRIDE;

  AppListItemGroupModel* model_;
  AppListItemViewListener* listener_;

  // Tiles per row.
  int tiles_per_row_;

  // Index of focused tile view.
  int focused_index_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemGroupView);
};

}  // namespace aura_shell

#endif  // ASH_APP_LIST_APP_LIST_ITEM_GROUP_VIEW_H_
