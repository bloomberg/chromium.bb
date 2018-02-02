// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/view.h"

namespace {

constexpr int kAvatarIconSize = 16;

gfx::Image SizeAndCircleIcon(const gfx::Image& icon) {
  return profiles::GetSizedAvatarIcon(icon, true, kAvatarIconSize,
                                      kAvatarIconSize, profiles::SHAPE_CIRCLE);
}

}  // namespace

DiceAccountsMenu::DiceAccountsMenu(const std::vector<AccountInfo>& accounts,
                                   const std::vector<gfx::Image>& icons)
    : menu_(this), accounts_(accounts) {
  DCHECK_EQ(accounts.size(), icons.size());
  gfx::Image default_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  // Add a menu item for each account.
  menu_.AddSeparator(ui::SPACING_SEPARATOR);
  for (size_t idx = 0; idx < accounts.size(); idx++) {
    menu_.AddItem(idx, base::UTF8ToUTF16(accounts[idx].email));
    menu_.SetIcon(
        menu_.GetIndexOfCommandId(idx),
        SizeAndCircleIcon(icons[idx].IsEmpty() ? default_icon : icons[idx]));
    menu_.AddSeparator(ui::SPACING_SEPARATOR);
  }
  // Add the "Use another account" button at the bottom.
  menu_.AddItem(
      accounts.size(),
      l10n_util::GetStringUTF16(IDS_PROFILES_DICE_USE_ANOTHER_ACCOUNT_BUTTON));
  menu_.SetIcon(menu_.GetIndexOfCommandId(accounts.size()),
                SizeAndCircleIcon(default_icon));
  menu_.AddSeparator(ui::SPACING_SEPARATOR);
}

void DiceAccountsMenu::Show(views::View* anchor_view) {
  DCHECK(!runner_);
  runner_ =
      std::make_unique<views::MenuRunner>(&menu_, views::MenuRunner::COMBOBOX);
  runner_->RunMenuAt(anchor_view->GetWidget(), nullptr,
                     anchor_view->GetBoundsInScreen(),
                     views::MENU_ANCHOR_BUBBLE_BELOW, ui::MENU_SOURCE_MOUSE);
}

DiceAccountsMenu::~DiceAccountsMenu() {}

bool DiceAccountsMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool DiceAccountsMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

void DiceAccountsMenu::ExecuteCommand(int id, int event_flags) {
  NOTIMPLEMENTED() << "Selected id: " << id;
}
