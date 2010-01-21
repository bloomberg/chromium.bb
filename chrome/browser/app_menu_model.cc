// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/app_menu_model.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

// TODO(akalin): Now that AppMenuModel handles the sync item
// dynamically, we don't need to refresh the menu on Windows/Linux.
// Remove that code and make sure it works.

AppMenuModel::AppMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                           Browser* browser)
    : menus::SimpleMenuModel(delegate),
      browser_(browser) {
  Build();
}

AppMenuModel::~AppMenuModel() {
}

bool AppMenuModel::IsLabelDynamicAt(int index) const {
  return IsSyncItem(index) || SimpleMenuModel::IsLabelDynamicAt(index);
}

string16 AppMenuModel::GetLabelAt(int index) const {
  return IsSyncItem(index) ?
      GetSyncMenuLabel() : SimpleMenuModel::GetLabelAt(index);
}

void AppMenuModel::Build() {
  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);
  AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);
  // Enumerate profiles asynchronously and then create the parent menu item.
  // We will create the child menu items for this once the asynchronous call is
  // done.  See OnGetProfilesDone().
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableUserDataDirProfiles)) {
    // Triggers profile list refresh in case it's changed.
    UserDataManager::Get()->RefreshUserDataDirProfiles();

    if (!profiles_menu_contents_.get()) {
      profiles_menu_contents_.reset(new menus::SimpleMenuModel(delegate()));
      BuildProfileSubMenu();
    }
    AddSubMenuWithStringId(IDS_PROFILE_MENU, profiles_menu_contents_.get());
  }

  AddSeparator();
  AddCheckItemWithStringId(IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR);
  AddItemWithStringId(IDC_FULLSCREEN, IDS_FULLSCREEN);
  AddSeparator();
  AddItemWithStringId(IDC_SHOW_HISTORY, IDS_SHOW_HISTORY);
  AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  AddItemWithStringId(IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS);

  // Create the manage extensions menu item.
  AddItemWithStringId(IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS);

  AddSeparator();

  // We assume that IsSyncEnabled() is constant for the lifetime of the
  // program (it just checks command-line flags).
  if (ProfileSyncService::IsSyncEnabled()) {
    AddItem(IDC_SYNC_BOOKMARKS, GetSyncMenuLabel());
    AddSeparator();
  }
#if defined(OS_MACOSX)
  AddItemWithStringId(IDC_OPTIONS, IDS_PREFERENCES_MAC);
#else
  AddItemWithStringId(IDC_OPTIONS, IDS_OPTIONS);
#endif

  if (browser_defaults::kShowAboutMenuItem) {
    AddItem(IDC_ABOUT,
            l10n_util::GetStringFUTF16(
                IDS_ABOUT,
                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  }
  AddItemWithStringId(IDC_HELP_PAGE, IDS_HELP_PAGE);
  if (browser_defaults::kShowExitMenuItem) {
    AddSeparator();
    AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  }
}

void AppMenuModel::BuildProfileSubMenu() {
  // Nothing to do if the menu has gone away.
  if (!profiles_menu_contents_.get())
    return;

  // Use the list of profiles in the browser.
  const std::vector<std::wstring>& profiles =
      browser_->user_data_dir_profiles();

  // Add direct submenu items for profiles.
  std::vector<std::wstring>::const_iterator iter = profiles.begin();
  for (int i = IDC_NEW_WINDOW_PROFILE_0;
       i <= IDC_NEW_WINDOW_PROFILE_LAST && iter != profiles.end();
       ++i, ++iter)
    profiles_menu_contents_->AddItem(i, WideToUTF16Hack(*iter));

  // If there are more profiles then show "Other" link.
  if (iter != profiles.end()) {
    profiles_menu_contents_->AddSeparator();
    profiles_menu_contents_->AddItemWithStringId(IDC_SELECT_PROFILE,
                                                 IDS_SELECT_PROFILE);
  }

  // Always show a link to select a new profile.
  profiles_menu_contents_->AddSeparator();
  profiles_menu_contents_->AddItemWithStringId(
      IDC_NEW_PROFILE,
      IDS_SELECT_PROFILE_DIALOG_NEW_PROFILE_ENTRY);
}

string16 AppMenuModel::GetSyncMenuLabel() const {
  return sync_ui_util::GetSyncMenuLabel(
      browser_->profile()->GetOriginalProfile()->GetProfileSyncService());
}

bool AppMenuModel::IsSyncItem(int index) const {
  return GetCommandIdAt(index) == IDC_SYNC_BOOKMARKS;
}
