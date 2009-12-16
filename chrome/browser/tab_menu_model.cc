// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/tab_menu_model.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#include "grit/generated_resources.h"

TabMenuModel::TabMenuModel(menus::SimpleMenuModel::Delegate* delegate)
    : menus::SimpleMenuModel(delegate) {
  Build();
}

void TabMenuModel::Build() {
  AddItemWithStringId(TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  // On Mac the HIG prefers "pin/unpin" to a checkmark. The Mac code will fix up
  // the actual string based on the tab's state via the delegate.
#if defined(OS_MACOSX)
  AddItemWithStringId(TabStripModel::CommandTogglePinned,
                      IDS_TAB_CXMENU_PIN_TAB);
#else
  AddCheckItemWithStringId(TabStripModel::CommandTogglePinned,
                           IDS_TAB_CXMENU_PIN_TAB);
#endif
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandCloseTab,
                      IDS_TAB_CXMENU_CLOSETAB);
  AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                      IDS_TAB_CXMENU_CLOSEOTHERTABS);
  AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                      IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  AddItemWithStringId(TabStripModel::CommandCloseTabsOpenedBy,
                      IDS_TAB_CXMENU_CLOSETABSOPENEDBY);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandRestoreTab, IDS_RESTORE_TAB);
  AddItemWithStringId(TabStripModel::CommandBookmarkAllTabs,
                      IDS_TAB_CXMENU_BOOKMARK_ALL_TABS);
}
