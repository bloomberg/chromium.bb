// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_GROUPS_VIEW_H_
#define ASH_APP_LIST_APP_LIST_GROUPS_VIEW_H_
#pragma once

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BoundsAnimator;
}

namespace ash {

class AppListItemGroupView;
class AppListItemViewListener;
class AppListModel;

// AppListGroupsView displays the UI for an AppListModel. If there are more than
// one group in the model , a button strip is displayed to allow user to switch
// between pages.
class ASH_EXPORT AppListGroupsView : public views::View,
                                            public views::ButtonListener,
                                            public ui::ListModelObserver {
 public:
  AppListGroupsView(AppListModel* model,
                    AppListItemViewListener* listener);
  virtual ~AppListGroupsView();

  // Gets current focused tile.
  views::View* GetFocusedTile() const;

 private:
  // Updates from model.
  void Update();

  // Adds a result group page.
  void AddPage(const std::string& title, AppListItemGroupView* page);

  // Gets preferred number of tiles per row.
  int GetPreferredTilesPerRow() const;

  // Gets current result page.
  AppListItemGroupView* GetCurrentPageView() const;

  // Sets current result page.
  void SetCurrentPage(int page);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from ListModelObserver:
  virtual void ListItemsAdded(int start, int count) OVERRIDE;
  virtual void ListItemsRemoved(int start, int count) OVERRIDE;
  virtual void ListItemsChanged(int start, int count) OVERRIDE;

  AppListModel* model_;  // Owned by parent AppListView.
  AppListItemViewListener* listener_;

  std::vector<AppListItemGroupView*> pages_;
  views::View* page_buttons_;
  int current_page_;

  scoped_ptr<views::BoundsAnimator> animator_;

  DISALLOW_COPY_AND_ASSIGN(AppListGroupsView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_GROUPS_VIEW_H_
