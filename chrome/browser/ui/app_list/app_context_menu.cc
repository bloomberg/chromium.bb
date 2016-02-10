// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_context_menu.h"

#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

AppContextMenu::AppContextMenu(AppContextMenuDelegate* delegate,
                               Profile* profile,
                               const std::string& app_id,
                               AppListControllerDelegate* controller)
    : delegate_(delegate),
      profile_(profile),
      app_id_(app_id),
      controller_(controller) {
}

AppContextMenu::~AppContextMenu() {
}

ui::MenuModel* AppContextMenu::GetMenuModel() {
  if (menu_model_.get())
    return menu_model_.get();

  menu_model_.reset(new ui::SimpleMenuModel(this));
  BuildMenu(menu_model_.get());
  return menu_model_.get();
}

void AppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  // Show Pin/Unpin option if shelf is available.
  if (controller_->GetPinnable(app_id()) != AppListControllerDelegate::NO_PIN) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItemWithStringId(
        TOGGLE_PIN,
        controller_->IsAppPinned(app_id_) ?
            IDS_APP_LIST_CONTEXT_MENU_UNPIN :
            IDS_APP_LIST_CONTEXT_MENU_PIN);
  }

  if (controller_->CanDoCreateShortcutsFlow()) {
    menu_model_->AddItemWithStringId(CREATE_SHORTCUTS,
                                     IDS_NEW_TAB_APP_CREATE_SHORTCUT);
  }
}

bool AppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PIN;
}

base::string16 AppContextMenu::GetLabelForCommandId(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    // Return "{Pin to, Unpin from} shelf" or "Pinned by administrator".
    // Note this only exists on Ash desktops.
    if (controller_->GetPinnable(app_id()) ==
        AppListControllerDelegate::PIN_FIXED) {
      return l10n_util::GetStringUTF16(
          IDS_APP_LIST_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY);
    }
    return controller_->IsAppPinned(app_id_) ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
  }

  NOTREACHED();
  return base::string16();
}

bool AppContextMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool AppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->GetPinnable(app_id_) ==
           AppListControllerDelegate::PIN_EDITABLE;
  }
  return true;
}

bool AppContextMenu::GetAcceleratorForCommandId(int command_id,
                                                ui::Accelerator* accelerator) {
  return false;
}

void AppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == TOGGLE_PIN &&
      controller_->GetPinnable(app_id_) ==
          AppListControllerDelegate::PIN_EDITABLE) {
    if (controller_->IsAppPinned(app_id_))
      controller_->UnpinApp(app_id_);
    else
      controller_->PinApp(app_id_);
  } else if (command_id == CREATE_SHORTCUTS) {
    controller_->DoCreateShortcutsFlow(profile_, app_id_);
  }
}

}  // namespace app_list
