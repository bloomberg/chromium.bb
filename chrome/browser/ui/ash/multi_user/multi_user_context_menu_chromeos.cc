// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"

#include "ash/multi_profile_uma.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

class MultiUserContextMenuChromeos : public ui::SimpleMenuModel,
                                     public ui::SimpleMenuModel::Delegate {
 public:
  explicit MultiUserContextMenuChromeos(aura::Window* window);
  virtual ~MultiUserContextMenuChromeos() {}

  // SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  // The window for which this menu is.
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserContextMenuChromeos);
};

MultiUserContextMenuChromeos::MultiUserContextMenuChromeos(aura::Window* window)
    : ui::SimpleMenuModel(this),
      window_(window) {
}

void MultiUserContextMenuChromeos::ExecuteCommand(int command_id,
                                                  int event_flags) {
  switch (command_id) {
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3: {
        ash::MultiProfileUMA::RecordTeleportAction(
            ash::MultiProfileUMA::TELEPORT_WINDOW_CAPTION_MENU);
        // When running the multi user mode on Chrome OS, windows can "visit"
        // another user's desktop.
        const std::string& user_id =
            ash::Shell::GetInstance()->session_state_delegate()->GetUserID(
                IDC_VISIT_DESKTOP_OF_LRU_USER_2 == command_id ? 1 : 2);
        chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
            window_,
            user_id);
        return;
      }
    default:
      NOTREACHED();
  }
}

}  // namespace

scoped_ptr<ui::MenuModel> CreateMultiUserContextMenu(
    aura::Window* window) {
  scoped_ptr<ui::MenuModel> model;
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  int logged_in_users = delegate->NumberOfLoggedInUsers();
  if (delegate && logged_in_users > 1) {
    // If this window is not owned, we don't show the menu addition.
    chrome::MultiUserWindowManager* manager =
        chrome::MultiUserWindowManager::GetInstance();
    const std::string user_id = manager->GetWindowOwner(window);
    if (user_id.empty() || !window ||
        manager->GetWindowOwner(window).empty())
      return model.Pass();
    MultiUserContextMenuChromeos* menu =
        new MultiUserContextMenuChromeos(window);
    model.reset(menu);
    for (int user_index = 1; user_index < logged_in_users; ++user_index) {
      menu->AddItem(
          user_index == 1 ? IDC_VISIT_DESKTOP_OF_LRU_USER_2 :
                            IDC_VISIT_DESKTOP_OF_LRU_USER_3,
          l10n_util::GetStringFUTF16(IDS_VISIT_DESKTOP_OF_LRU_USER,
                                     delegate->GetUserDisplayName(
                                         user_index)));
    }
  }
  return model.Pass();
}
