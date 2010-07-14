// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_menu_model.h"

#include "base/command_line.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"

TabMenuModel::TabMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                           bool is_pinned,
                           bool allow_toolbar_toggle,
                           bool is_toolbar_visible)
    : menus::SimpleMenuModel(delegate) {
  Build(is_pinned, allow_toolbar_toggle, is_toolbar_visible);
}

// static
bool TabMenuModel::AreVerticalTabsEnabled() {
#if defined(OS_CHROMEOS)
  return true;
#elif defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVerticalTabs);
#else
  return false;
#endif
}

void TabMenuModel::Build(bool is_pinned, bool allow_toolbar_toggle,
                         bool is_toolbar_visible) {
  AddItemWithStringId(TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB);
  AddSeparator();
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  AddItemWithStringId(
      TabStripModel::CommandTogglePinned,
      is_pinned ? IDS_TAB_CXMENU_UNPIN_TAB : IDS_TAB_CXMENU_PIN_TAB);
  if (allow_toolbar_toggle) {
    AddItemWithStringId(
        TabStripModel::CommandToggleToolbar,
        is_toolbar_visible ? IDS_TAB_CXMENU_HIDE_TOOLBAR :
                             IDS_TAB_CXMENU_SHOW_TOOLBAR);
  }
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
