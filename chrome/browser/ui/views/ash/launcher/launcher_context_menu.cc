// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_context_menu.h"

#include "ash/launcher/launcher_context_menu.h"
#include "ash/shell.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LauncherContextMenu::LauncherContextMenu(ChromeLauncherDelegate* delegate,
                                         const ash::LauncherItem* item)
    : ui::SimpleMenuModel(NULL),
      delegate_(delegate),
      item_(item ? *item : ash::LauncherItem()) {
  set_delegate(this);

  if (is_valid_item()) {
    if (item_.type == ash::TYPE_APP_SHORTCUT) {
      DCHECK(delegate->IsPinned(item_.id));
      AddItem(
          MENU_PIN,
          l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_UNPIN));
      AddSeparator();
      AddCheckItemWithStringId(
          LAUNCH_TYPE_REGULAR_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
      AddCheckItemWithStringId(
          LAUNCH_TYPE_PINNED_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_PINNED);
      AddCheckItemWithStringId(
          LAUNCH_TYPE_WINDOW,
          IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
      // Even though the launch type is Full Screen it is more accurately
      // described as Maximized in Ash.
      AddCheckItemWithStringId(
          LAUNCH_TYPE_FULLSCREEN,
          IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
    } else if (item_.type == ash::TYPE_BROWSER_SHORTCUT) {
      AddItem(MENU_NEW_WINDOW,
              l10n_util::GetStringUTF16(IDS_LAUNCHER_NEW_WINDOW));
      if (!delegate_->IsLoggedInAsGuest()) {
        AddItem(MENU_NEW_INCOGNITO_WINDOW,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_NEW_INCOGNITO_WINDOW));
      }
    } else {
      AddItem(MENU_OPEN, delegate->GetTitle(item_));
      if (delegate->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
    }
    AddSeparator();
  }
  AddCheckItemWithStringId(
      MENU_AUTO_HIDE, ash::LauncherContextMenu::GetAutoHideResourceStringId());
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case LAUNCH_TYPE_PINNED_TAB:
      return delegate_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_PINNED;
    case LAUNCH_TYPE_REGULAR_TAB:
      return delegate_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_REGULAR;
    case LAUNCH_TYPE_WINDOW:
      return delegate_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_WINDOW;
    case LAUNCH_TYPE_FULLSCREEN:
      return delegate_->GetLaunchType(item_.id) ==
          ExtensionPrefs::LAUNCH_FULLSCREEN;
    case MENU_AUTO_HIDE:
      return ash::LauncherContextMenu::IsAutoHideMenuHideChecked();
    default:
      return false;
  }
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool LauncherContextMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void LauncherContextMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN:
      delegate_->Open(item_.id);
      break;
    case MENU_CLOSE:
      delegate_->Close(item_.id);
      break;
    case MENU_PIN:
      delegate_->TogglePinned(item_.id);
      break;
    case LAUNCH_TYPE_PINNED_TAB:
      delegate_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_PINNED);
      break;
    case LAUNCH_TYPE_REGULAR_TAB:
      delegate_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_REGULAR);
      break;
    case LAUNCH_TYPE_WINDOW:
      delegate_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_WINDOW);
      break;
    case LAUNCH_TYPE_FULLSCREEN:
      delegate_->SetLaunchType(item_.id, ExtensionPrefs::LAUNCH_FULLSCREEN);
      break;
    case MENU_AUTO_HIDE:
      delegate_->SetAutoHideBehavior(
          ash::LauncherContextMenu::GetToggledAutoHideBehavior());
      break;
    case MENU_NEW_WINDOW:
      delegate_->CreateNewWindow();
      break;
    case MENU_NEW_INCOGNITO_WINDOW:
      delegate_->CreateNewIncognitoWindow();
      break;
  }
}
