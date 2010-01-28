// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_TAB_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"

class Browser;

// A menu model that builds the contents of the tab context menu. This menu has
// only one level (no submenus).
class TabMenuModel : public menus::SimpleMenuModel {
 public:
  explicit TabMenuModel(menus::SimpleMenuModel::Delegate* delegate);
  virtual ~TabMenuModel() {}

 private:
  void Build();

  DISALLOW_COPY_AND_ASSIGN(TabMenuModel);
};

#endif  // CHROME_BROWSER_TAB_MENU_MODEL_H_
