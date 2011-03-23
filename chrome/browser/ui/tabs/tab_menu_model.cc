// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "base/command_line.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"

TabMenuModel::TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           bool is_pinned)
    : ui::SimpleMenuModel(delegate) {
  Build(is_pinned);
}

TabMenuModel::TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           TabStripModel* tab_strip,
                           int index)
    : ui::SimpleMenuModel(delegate) {
  Build(tab_strip, index);
}

// static
bool TabMenuModel::AreVerticalTabsEnabled() {
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVerticalTabs);
#else
  return false;
#endif
}

void TabMenuModel::Build(bool is_pinned) {
  AddItemWithStringId(TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  AddItemWithStringId(
      TabStripModel::CommandTogglePinned,
      is_pinned ? IDS_TAB_CXMENU_UNPIN_TAB : IDS_TAB_CXMENU_PIN_TAB);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandCloseTab,
                      IDS_TAB_CXMENU_CLOSETAB);
  AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                      IDS_TAB_CXMENU_CLOSEOTHERTABS);
  AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                      IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandRestoreTab, IDS_RESTORE_TAB);
  AddItemWithStringId(TabStripModel::CommandBookmarkAllTabs,
                      IDS_TAB_CXMENU_BOOKMARK_ALL_TABS);
  if (AreVerticalTabsEnabled()) {
    AddSeparator();
    AddCheckItemWithStringId(TabStripModel::CommandUseVerticalTabs,
                             IDS_TAB_CXMENU_USE_VERTICAL_TABS);
  }
}

void TabMenuModel::Build(TabStripModel* tab_strip, int index) {
  bool effects_multiple_tabs =
      (tab_strip->IsTabSelected(index) &&
       tab_strip->selection_model().selected_indices().size() > 1);
  AddItemWithStringId(TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  bool will_pin = tab_strip->WillContextMenuPin(index);
  if (effects_multiple_tabs) {
    AddItemWithStringId(
        TabStripModel::CommandTogglePinned,
        will_pin ? IDS_TAB_CXMENU_PIN_TABS : IDS_TAB_CXMENU_UNPIN_TABS);
  } else {
    AddItemWithStringId(
        TabStripModel::CommandTogglePinned,
        will_pin ? IDS_TAB_CXMENU_PIN_TAB : IDS_TAB_CXMENU_UNPIN_TAB);
  }
  AddSeparator();
  if (effects_multiple_tabs) {
    AddItemWithStringId(TabStripModel::CommandCloseTab,
                        IDS_TAB_CXMENU_CLOSETABS);
  } else {
    AddItemWithStringId(TabStripModel::CommandCloseTab,
                        IDS_TAB_CXMENU_CLOSETAB);
  }
  AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                      IDS_TAB_CXMENU_CLOSEOTHERTABS);
  AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                      IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandRestoreTab, IDS_RESTORE_TAB);
  AddItemWithStringId(TabStripModel::CommandBookmarkAllTabs,
                      IDS_TAB_CXMENU_BOOKMARK_ALL_TABS);
  if (AreVerticalTabsEnabled()) {
    AddSeparator();
    AddCheckItemWithStringId(TabStripModel::CommandUseVerticalTabs,
                             IDS_TAB_CXMENU_USE_VERTICAL_TABS);
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTabGroupsContextMenu)) {
    AddSeparator();
    AddItemWithStringId(TabStripModel::CommandSelectByDomain,
                        IDS_TAB_CXMENU_SELECT_BY_DOMAIN);
    AddItemWithStringId(TabStripModel::CommandSelectByOpener,
                        IDS_TAB_CXMENU_SELECT_BY_OPENER);
  }
}
