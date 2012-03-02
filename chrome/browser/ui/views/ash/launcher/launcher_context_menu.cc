// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_context_menu.h"

#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LauncherContextMenu::LauncherContextMenu(ChromeLauncherDelegate* delegate,
                                         ash::LauncherID id)
    : ui::SimpleMenuModel(NULL),
      delegate_(delegate),
      id_(id) {
  set_delegate(this);
  ash::LauncherItem item;
  item.id = id;
  AddItem(MENU_OPEN,
          delegate->GetTitle(item));
  AddItem(MENU_PIN,
          delegate->IsPinned(id) ?
              l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_UNPIN) :
              l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_PIN));
  if (delegate->IsOpen(id)) {
    AddItem(MENU_CLOSE,
            l10n_util::GetStringUTF16(IDS_LAUNCHER_CONTEXT_MENU_CLOSE));
  }
}

LauncherContextMenu::~LauncherContextMenu() {
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN:
      return true;
    case MENU_CLOSE:
      return true;
    case MENU_PIN:
      return delegate_->IsPinnable(id_);
  }
  return false;
}

bool LauncherContextMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void LauncherContextMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_OPEN:
      delegate_->Open(id_);
      break;
    case MENU_CLOSE:
      delegate_->Close(id_);
      break;
    case MENU_PIN:
      delegate_->TogglePinned(id_);
      break;
  }
}
