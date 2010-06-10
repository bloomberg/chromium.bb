// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_menu_model.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

AppMenuModel::AppMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                           Browser* browser)
    : menus::SimpleMenuModel(delegate),
      browser_(browser) {
  Build();
}

AppMenuModel::~AppMenuModel() {
}

bool AppMenuModel::IsLabelDynamicAt(int index) const {
  return IsDynamicItem(index) || SimpleMenuModel::IsLabelDynamicAt(index);
}

string16 AppMenuModel::GetLabelAt(int index) const {
  if (!IsDynamicItem(index))
    return SimpleMenuModel::GetLabelAt(index);

  int command_id = GetCommandIdAt(index);

  switch (command_id) {
    case IDC_ABOUT: return GetAboutEntryMenuLabel(); break;
    case IDC_SYNC_BOOKMARKS: return GetSyncMenuLabel(); break;
    default:
      NOTREACHED();
      return string16();
  }
}

bool AppMenuModel::GetIconAt(int index, SkBitmap* icon) const {
  if (GetCommandIdAt(index) == IDC_ABOUT &&
      Singleton<UpgradeDetector>::get()->notify_upgrade()) {
    // Show the exclamation point next to the menu item.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *icon = *rb.GetBitmapNamed(IDR_UPDATE_AVAILABLE);
    return true;
  }
  return false;
}

void AppMenuModel::Build() {
  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);
  AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);

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

  // The user is always signed in to sync on Chrome OS, so there's no sense
  // showing this label.
#if !defined(OS_CHROMEOS)
  // We assume that IsSyncEnabled() is constant for the lifetime of the
  // program (it just checks command-line flags).
  if (ProfileSyncService::IsSyncEnabled()) {
    AddItem(IDC_SYNC_BOOKMARKS, GetSyncMenuLabel());
    AddSeparator();
  }
#endif

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
#if defined(OS_CHROMEOS)
    AddItemWithStringId(IDC_EXIT, IDS_SIGN_OUT);
#else
    AddItemWithStringId(IDC_EXIT, IDS_EXIT);
#endif
  }
}

string16 AppMenuModel::GetSyncMenuLabel() const {
  return sync_ui_util::GetSyncMenuLabel(
      browser_->profile()->GetOriginalProfile()->GetProfileSyncService());
}

string16 AppMenuModel::GetAboutEntryMenuLabel() const {
  if (Singleton<UpgradeDetector>::get()->notify_upgrade()) {
    return l10n_util::GetStringFUTF16(
        IDS_UPDATE_NOW, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ABOUT, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

bool AppMenuModel::IsDynamicItem(int index) const {
  int command_id = GetCommandIdAt(index);
  return command_id == IDC_SYNC_BOOKMARKS ||
         command_id == IDC_ABOUT;
}
