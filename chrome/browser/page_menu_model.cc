// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/page_menu_model.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

PageMenuModel::PageMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                             Browser* browser)
    : menus::SimpleMenuModel(delegate), browser_(browser) {
  Build();
}

void PageMenuModel::Build() {
#if !defined(OS_CHROMEOS)
  AddItemWithStringId(IDC_CREATE_SHORTCUTS, IDS_CREATE_SHORTCUTS);
  AddSeparator();
#endif
  AddItemWithStringId(IDC_CUT, IDS_CUT);
  AddItemWithStringId(IDC_COPY, IDS_COPY);
  AddItemWithStringId(IDC_PASTE, IDS_PASTE);
  AddSeparator();
  AddItemWithStringId(IDC_FIND, IDS_FIND);
#if !defined(OS_CHROMEOS)
  AddItemWithStringId(IDC_SAVE_PAGE, IDS_SAVE_PAGE);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);
#endif
  AddSeparator();

  zoom_menu_model_.reset(new ZoomMenuModel(delegate()));
  AddSubMenuWithStringId(IDS_ZOOM_MENU, zoom_menu_model_.get());

  encoding_menu_model_.reset(new EncodingMenuModel(browser_));
  AddSubMenuWithStringId(IDS_ENCODING_MENU, encoding_menu_model_.get());

  AddSeparator();
  devtools_menu_model_.reset(new DevToolsMenuModel(delegate()));
  AddSubMenuWithStringId(IDS_DEVELOPER_MENU, devtools_menu_model_.get());

  AddSeparator();
  AddItemWithStringId(IDC_REPORT_BUG, IDS_REPORT_BUG);
}

////////////////////////////////////////////////////////////////////////////////
// EncodingMenuModel

EncodingMenuModel::EncodingMenuModel(Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(menus::SimpleMenuModel(this)),
      browser_(browser) {
  Build();
}

void EncodingMenuModel::Build() {
  EncodingMenuController::EncodingMenuItemList encoding_menu_items;
  EncodingMenuController encoding_menu_controller;
  encoding_menu_controller.GetEncodingMenuItems(browser_->profile(),
                                                &encoding_menu_items);

  int group_id = 0;
  EncodingMenuController::EncodingMenuItemList::iterator it =
      encoding_menu_items.begin();
  for (; it != encoding_menu_items.end(); ++it) {
    int id = it->first;
    string16& label = it->second;
    if (id == 0) {
      AddSeparator();
    } else {
      if (id == IDC_ENCODING_AUTO_DETECT) {
        AddCheckItem(id, label);
      } else {
        // Use the id of the first radio command as the id of the group.
        if (group_id <= 0)
          group_id = id;
        AddRadioItem(id, label, group_id);
      }
    }
  }
}

bool EncodingMenuModel::IsCommandIdChecked(int command_id) const {
  TabContents* current_tab = browser_->GetSelectedTabContents();
  EncodingMenuController controller;
  return controller.IsItemChecked(browser_->profile(),
                                  current_tab->encoding(), command_id);
}

bool EncodingMenuModel::IsCommandIdEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool EncodingMenuModel::GetAcceleratorForCommandId(
    int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void EncodingMenuModel::ExecuteCommand(int command_id) {
  browser_->ExecuteCommand(command_id);
}

////////////////////////////////////////////////////////////////////////////////
// ZoomMenuModel

ZoomMenuModel::ZoomMenuModel(menus::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  Build();
}

void ZoomMenuModel::Build() {
  AddItemWithStringId(IDC_ZOOM_PLUS, IDS_ZOOM_PLUS);
  AddItemWithStringId(IDC_ZOOM_NORMAL, IDS_ZOOM_NORMAL);
  AddItemWithStringId(IDC_ZOOM_MINUS, IDS_ZOOM_MINUS);
}

////////////////////////////////////////////////////////////////////////////////
// DevToolsMenuModel

DevToolsMenuModel::DevToolsMenuModel(menus::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  Build();
}

void DevToolsMenuModel::Build() {
  AddItemWithStringId(IDC_VIEW_SOURCE, IDS_VIEW_SOURCE);
  if (g_browser_process->have_inspector_files()) {
    AddItemWithStringId(IDC_DEV_TOOLS, IDS_DEV_TOOLS);
    AddItemWithStringId(IDC_DEV_TOOLS_CONSOLE, IDS_DEV_TOOLS_CONSOLE);
  }
  AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
}
