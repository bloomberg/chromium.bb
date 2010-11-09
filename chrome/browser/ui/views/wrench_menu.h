// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WRENCH_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_WRENCH_MENU_H_
#pragma once

#include <map>
#include <utility>

#include "app/menus/menu_model.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "views/controls/menu/menu_delegate.h"

class Browser;

namespace views {
class MenuButton;
class MenuItemView;
class View;
}  // namespace views

// WrenchMenu adapts the WrenchMenuModel to view's menu related classes.
class WrenchMenu : public base::RefCounted<WrenchMenu>,
                   public views::MenuDelegate {
 public:
  explicit WrenchMenu(Browser* browser);

  void Init(menus::MenuModel* model);

  // Shows the menu relative to the specified view.
  void RunMenu(views::MenuButton* host);

  // MenuDelegate overrides:
  virtual bool IsItemChecked(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual void ExecuteCommand(int id);
  virtual bool GetAccelerator(int id, views::Accelerator* accelerator);

 private:
  friend class base::RefCounted<WrenchMenu>;

  class CutCopyPasteView;
  class ZoomView;

  typedef std::pair<menus::MenuModel*,int> Entry;
  typedef std::map<int,Entry> IDToEntry;

  ~WrenchMenu();

  // Populates |parent| with all the child menus in |model|. Recursively invokes
  // |PopulateMenu| for any submenu. |next_id| is incremented for every menu
  // that is created.
  void PopulateMenu(views::MenuItemView* parent,
                    menus::MenuModel* model,
                    int* next_id);

  // Adds a new menu to |parent| to represent the MenuModel/index pair passed
  // in.
  views::MenuItemView* AppendMenuItem(views::MenuItemView* parent,
                                      menus::MenuModel* model,
                                      int index,
                                      menus::MenuModel::ItemType menu_type,
                                      int* next_id);

  // Invoked from the cut/copy/paste menus. Cancels the current active menu and
  // activates the menu item in |model| at |index|.
  void CancelAndEvaluate(menus::MenuModel* model, int index);

  // The views menu.
  scoped_ptr<views::MenuItemView> root_;

  // Maps from the ID as understood by MenuItemView to the model/index pair the
  // item came from.
  IDToEntry id_to_entry_;

  // Browser the menu is being shown for.
  Browser* browser_;

  // |CancelAndEvaluate| sets |selected_menu_model_| and |selected_index_|.
  // If |selected_menu_model_| is non-null after the menu completes
  // ActivatedAt is invoked. This is done so that ActivatedAt isn't invoked
  // while the message loop is nested.
  menus::MenuModel* selected_menu_model_;
  int selected_index_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WRENCH_MENU_H_
