// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_context_menu.h"

#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LauncherContextMenu::LauncherContextMenu(ChromeLauncherDelegate* delegate,
                                         const ash::LauncherItem* item)
    : ui::SimpleMenuModel(NULL),
      delegate_(delegate),
      item_(item ? *item : ash::LauncherItem()),
      shelf_menu_(delegate) {
  set_delegate(this);

  if (is_valid_item()) {
    if (item_.type == ash::TYPE_APP_SHORTCUT) {
      DCHECK(delegate->IsPinned(item_.id));
      AddItem(
          MENU_PIN,
          l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_UNPIN));
      AddCheckItemWithStringId(
          LAUNCH_TYPE_REGULAR_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
      AddCheckItemWithStringId(
          LAUNCH_TYPE_WINDOW,
          IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
    } else {
      AddItem(MENU_OPEN, delegate->GetTitle(item_));
      if (delegate->IsOpen(item_.id)) {
        AddItem(MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
      }
    }
    AddSeparator();
  }
  AddSubMenuWithStringId(MENU_AUTO_HIDE,
                         IDS_LAUNCHER_CONTEXT_MENU_LAUNCHER_VISIBILITY,
                         &shelf_menu_);
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case LAUNCH_TYPE_REGULAR_TAB:
      return delegate_->GetAppType(item_.id) ==
          ChromeLauncherDelegate::APP_TYPE_TAB;
    case LAUNCH_TYPE_WINDOW:
      return delegate_->GetAppType(item_.id) ==
          ChromeLauncherDelegate::APP_TYPE_WINDOW;
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
    case LAUNCH_TYPE_REGULAR_TAB:
      delegate_->SetAppType(item_.id, ChromeLauncherDelegate::APP_TYPE_TAB);
      break;
    case LAUNCH_TYPE_WINDOW:
      delegate_->SetAppType(item_.id, ChromeLauncherDelegate::APP_TYPE_WINDOW);
      break;
    case MENU_AUTO_HIDE:
      break;
  }
}
