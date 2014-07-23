// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"

#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_warning_dialog.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace chromeos {

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
  ExecuteVisitDesktopCommand(command_id, window_);
}
}  // namespace
}  // namespace chromeos

scoped_ptr<ui::MenuModel> CreateMultiUserContextMenu(aura::Window* window) {
  scoped_ptr<ui::MenuModel> model;
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  if (!delegate)
    return model.Pass();

  int logged_in_users = delegate->NumberOfLoggedInUsers();
  if (logged_in_users > 1) {
    // If this window is not owned, we don't show the menu addition.
    chrome::MultiUserWindowManager* manager =
        chrome::MultiUserWindowManager::GetInstance();
    const std::string user_id = manager->GetWindowOwner(window);
    if (user_id.empty() || !window)
      return model.Pass();
    chromeos::MultiUserContextMenuChromeos* menu =
        new chromeos::MultiUserContextMenuChromeos(window);
    model.reset(menu);
    for (int user_index = 1; user_index < logged_in_users; ++user_index) {
      const user_manager::UserInfo* user_info =
          delegate->GetUserInfo(user_index);
      menu->AddItem(user_index == 1 ? IDC_VISIT_DESKTOP_OF_LRU_USER_2
                                    : IDC_VISIT_DESKTOP_OF_LRU_USER_3,
                    l10n_util::GetStringFUTF16(
                        IDS_VISIT_DESKTOP_OF_LRU_USER,
                        user_info->GetDisplayName(),
                        base::ASCIIToUTF16(user_info->GetEmail())));
    }
  }
  return model.Pass();
}

void OnAcceptTeleportWarning(
    const std::string user_id, aura::Window* window_, bool no_show_again) {
  PrefService* pref = ProfileManager::GetActiveUserProfile()->GetPrefs();
  pref->SetBoolean(prefs::kMultiProfileWarningShowDismissed, no_show_again);

  ash::MultiProfileUMA::RecordTeleportAction(
      ash::MultiProfileUMA::TELEPORT_WINDOW_CAPTION_MENU);

  chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(window_,
                                                                   user_id);
}

void ExecuteVisitDesktopCommand(int command_id, aura::Window* window) {
  switch (command_id) {
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3: {
      // When running the multi user mode on Chrome OS, windows can "visit"
      // another user's desktop.
      const std::string& user_id =
          ash::Shell::GetInstance()
              ->session_state_delegate()
              ->GetUserInfo(IDC_VISIT_DESKTOP_OF_LRU_USER_2 == command_id ? 1
                                                                          : 2)
              ->GetUserID();
      base::Callback<void(bool)> on_accept =
          base::Bind(&OnAcceptTeleportWarning, user_id, window);

      // Don't show warning dialog if any logged in user in multi-profiles
      // session dismissed it.
      const user_manager::UserList logged_in_users =
          chromeos::UserManager::Get()->GetLoggedInUsers();
      for (user_manager::UserList::const_iterator it = logged_in_users.begin();
           it != logged_in_users.end();
           ++it) {
        if (multi_user_util::GetProfileFromUserID(
            multi_user_util::GetUserIDFromEmail((*it)->email()))->GetPrefs()->
            GetBoolean(prefs::kMultiProfileWarningShowDismissed)) {
          bool active_user_show_option =
              ProfileManager::GetActiveUserProfile()->
              GetPrefs()->GetBoolean(prefs::kMultiProfileWarningShowDismissed);
          on_accept.Run(active_user_show_option);
          return;
        }
      }
      chromeos::ShowMultiprofilesWarningDialog(on_accept);
      return;
    }
    default:
      NOTREACHED();
  }
}
