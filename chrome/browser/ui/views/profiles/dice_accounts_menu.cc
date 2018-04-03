// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/view.h"

namespace {

constexpr int kAvatarIconSize = 16;

// Used to identify the "Use another account" button.
constexpr int kUseAnotherAccountCmdId = std::numeric_limits<int>::max();

// Anchor inset used to position the accounts menu.
constexpr int kAnchorInset = 8;

// TODO(tangltom): Calculate these values considering the existing menu config.
constexpr int kVerticalImagePadding = 9;
constexpr int kHorizontalImagePadding = 6;

gfx::Image SizeAndCircleIcon(const gfx::Image& icon) {
  gfx::Image circled_icon = profiles::GetSizedAvatarIcon(
      icon, true, kAvatarIconSize, kAvatarIconSize, profiles::SHAPE_CIRCLE);

  return gfx::Image(gfx::CanvasImageSource::CreatePadded(
      *circled_icon.ToImageSkia(),
      gfx::Insets(kVerticalImagePadding, kHorizontalImagePadding)));
}

}  // namespace

DiceAccountsMenu::DiceAccountsMenu(const std::vector<AccountInfo>& accounts,
                                   const std::vector<gfx::Image>& icons,
                                   Callback account_selected_callback)
    : menu_(this),
      accounts_(accounts),
      account_selected_callback_(std::move(account_selected_callback)) {
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
  }
  // Add the "Use another account" button at the bottom.
  menu_.AddItem(
      kUseAnotherAccountCmdId,
      l10n_util::GetStringUTF16(IDS_PROFILES_DICE_USE_ANOTHER_ACCOUNT_BUTTON));
  menu_.SetIcon(menu_.GetIndexOfCommandId(kUseAnotherAccountCmdId),
                SizeAndCircleIcon(default_icon));
  menu_.AddSeparator(ui::SPACING_SEPARATOR);
}

void DiceAccountsMenu::Show(views::View* anchor_view,
                            views::MenuButton* menu_button) {
  DCHECK(!runner_);
  runner_ =
      std::make_unique<views::MenuRunner>(&menu_, views::MenuRunner::COMBOBOX);
  // Calculate custom anchor bounds to position the menu.
  // The menu is aligned along the right edge (left edge in RTL mode) of the
  // anchor, slightly shifted inside by |kAnchorInset| and overlapping
  // |anchor_view| on the bottom by |kAnchorInset|. |anchor_bounds| is collapsed
  // so the menu only takes the width it needs.
  gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();
  anchor_bounds.Inset(kAnchorInset, kAnchorInset);
#if defined(OS_MACOSX)
  // On Mac, menus align to the left of the anchor, so collapse the right side
  // of the rect.
  bool collapse_right = true;
#else
  bool collapse_right = false;
#endif
  if (base::i18n::IsRTL())
    collapse_right = !collapse_right;

  if (collapse_right)
    anchor_bounds.Inset(0, 0, anchor_bounds.width(), 0);
  else
    anchor_bounds.Inset(anchor_bounds.width(), 0, 0, 0);

  runner_->RunMenuAt(anchor_view->GetWidget(), menu_button, anchor_bounds,
                     views::MENU_ANCHOR_TOPRIGHT, ui::MENU_SOURCE_MOUSE);
}

DiceAccountsMenu::~DiceAccountsMenu() {}

bool DiceAccountsMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool DiceAccountsMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

void DiceAccountsMenu::ExecuteCommand(int id, int event_flags) {
  DCHECK((0 <= id && static_cast<size_t>(id) < accounts_.size()) ||
         id == kUseAnotherAccountCmdId);
  base::Optional<AccountInfo> account;
  if (id != kUseAnotherAccountCmdId)
    account = accounts_[id];
  std::move(account_selected_callback_).Run(account);
}
