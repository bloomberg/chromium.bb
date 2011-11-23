// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_menu_model.h"

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

void WrenchMenuModel::Build() {
  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession))
    AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);

  AddSeparator();
  CreateCutCopyPaste();

  AddSeparator();
  CreateZoomFullscreen();

  AddSeparator();
  AddItemWithStringId(IDC_SAVE_PAGE, IDS_SAVE_PAGE);
  AddItemWithStringId(IDC_FIND, IDS_FIND);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);

  tools_menu_model_.reset(new ToolsMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_TOOLS_MENU,
      tools_menu_model_.get());

  AddSeparator();

  bookmark_sub_menu_model_.reset(new BookmarkSubMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_BOOKMARKS_MENU, IDS_BOOKMARKS_MENU,
      bookmark_sub_menu_model_.get());
  AddItemWithStringId(IDC_SHOW_HISTORY, IDS_SHOW_HISTORY);
  AddItemWithStringId(IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS);

  AddSeparator();

  AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);
  const string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME);
  AddItem(IDC_ABOUT, l10n_util::GetStringFUTF16(IDS_ABOUT, product_name));
  string16 num_background_pages = base::FormatNumber(
      TaskManager::GetBackgroundPageCount());
  AddItem(IDC_VIEW_BACKGROUND_PAGES,
      l10n_util::GetStringFUTF16(IDS_VIEW_BACKGROUND_PAGES,
          num_background_pages));
  AddItem(IDC_UPGRADE_DIALOG,
      l10n_util::GetStringFUTF16(IDS_UPDATE_NOW, product_name));
  AddItem(IDC_VIEW_INCOMPATIBILITIES,
      l10n_util::GetStringUTF16(IDS_VIEW_INCOMPATIBILITIES));

  // Use an icon for IDC_HELP_PAGE menu item.
  AddItemWithStringId(IDC_HELP_PAGE, IDS_HELP_PAGE);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(GetIndexOfCommandId(IDC_HELP_PAGE),
          *rb.GetBitmapNamed(IDR_HELP_MENU));

  // Show IDC_FEEDBACK in top-tier wrench menu for ChromeOS.
  AddItemWithStringId(IDC_FEEDBACK, IDS_FEEDBACK);

  AddGlobalErrorMenuItems();

  AddSeparator();

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    AddItemWithStringId(IDC_EXIT, IDS_EXIT_GUEST_MODE);
  } else {
    AddItemWithStringId(IDC_EXIT, IDS_SIGN_OUT);
  }
}

