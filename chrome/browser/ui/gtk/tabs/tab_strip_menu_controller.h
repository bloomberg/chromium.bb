// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_TAB_STRIP_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_GTK_TABS_TAB_STRIP_MENU_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"

class TabGtk;
class TabStripModel;

namespace gfx {
class Point;
}

namespace ui {
class Accelerator;
}

// Controls the context menu of a specific tab. It is created every time a right
// click occurs on a tab.
class TabStripMenuController : public ui::SimpleMenuModel::Delegate,
                               public MenuGtk::Delegate {
 public:
  // |tab| is the tab for which this menu is created. |model| is the
  // TabStripModel where this tab belongs to. |index| is the index of |tab|
  // within the tab strip.
  TabStripMenuController(TabGtk* tab, TabStripModel* model, int index);
  virtual ~TabStripMenuController();
  void RunMenu(const gfx::Point& point, guint32 event_time);
  void Cancel();

 private:
  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // Overridden from MenuGtk::Delegate:
  virtual GtkWidget* GetImageForCommandId(int command_id) const OVERRIDE;

  // The context menu.
  scoped_ptr<MenuGtk> menu_;

  // Weak pointer to the tab for which the context menu was brought up for. Set
  // to NULL when the menu is canceled.
  TabGtk* tab_;

  // The model.
  TabMenuModel model_;

  DISALLOW_COPY_AND_ASSIGN(TabStripMenuController);
};

#endif  // CHROME_BROWSER_UI_GTK_TABS_TAB_STRIP_MENU_CONTROLLER_H_
