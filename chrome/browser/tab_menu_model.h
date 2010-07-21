// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_TAB_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"

class Browser;

// A menu model that builds the contents of the tab context menu. This menu has
// only one level (no submenus). TabMenuModel caches local state from the
// tab (such as the pinned state). To make sure the menu reflects the real state
// of the tab a new TabMenuModel should be created each time the menu is shown.
class TabMenuModel : public menus::SimpleMenuModel {
 public:
  TabMenuModel(menus::SimpleMenuModel::Delegate* delegate, bool is_pinned);
  virtual ~TabMenuModel() {}

  // Returns true if vertical tabs are enabled.
  static bool AreVerticalTabsEnabled();

 private:
  void Build(bool is_pinned);

  DISALLOW_COPY_AND_ASSIGN(TabMenuModel);
};

#endif  // CHROME_BROWSER_TAB_MENU_MODEL_H_
