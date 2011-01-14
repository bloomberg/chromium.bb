// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"

// A menu model that builds the contents of the tab context menu. This menu has
// only one level (no submenus). TabMenuModel caches local state from the
// tab (such as the pinned state). To make sure the menu reflects the real state
// of the tab a new TabMenuModel should be created each time the menu is shown.
class TabMenuModel : public ui::SimpleMenuModel {
 public:
  TabMenuModel(ui::SimpleMenuModel::Delegate* delegate, bool is_pinned);
  virtual ~TabMenuModel() {}

  // Returns true if vertical tabs are enabled.
  static bool AreVerticalTabsEnabled();

 private:
  void Build(bool is_pinned);

  DISALLOW_COPY_AND_ASSIGN(TabMenuModel);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
