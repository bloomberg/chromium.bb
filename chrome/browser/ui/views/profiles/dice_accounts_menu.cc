// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"

#include "base/strings/utf_string_conversions.h"
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

// TODO(tangltom): Calculate these values considering the existing menu config
constexpr int kVerticalImagePadding = 9;
constexpr int kHorizontalImagePadding = 6;

class PaddedImageSource : public gfx::CanvasImageSource {
 public:
  explicit PaddedImageSource(const gfx::Image& image)
      : CanvasImageSource(gfx::Size(image.Width() + 2 * kHorizontalImagePadding,
                                    image.Height() + 2 * kVerticalImagePadding),
                          false),
        image_(image) {}

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(*image_.ToImageSkia(), kHorizontalImagePadding,
                         kVerticalImagePadding);
  }

 private:
  gfx::Image image_;

  DISALLOW_COPY_AND_ASSIGN(PaddedImageSource);
};

gfx::Image SizeAndCircleIcon(const gfx::Image& icon) {
  gfx::Image circled_icon = profiles::GetSizedAvatarIcon(
      icon, true, kAvatarIconSize, kAvatarIconSize, profiles::SHAPE_CIRCLE);

  gfx::Image padded_icon = gfx::Image(gfx::ImageSkia(
      std::make_unique<PaddedImageSource>(circled_icon), 1 /* scale */));
  return padded_icon;
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

void DiceAccountsMenu::Show(views::View* anchor_view) {
  DCHECK(!runner_);
  runner_ = std::make_unique<views::MenuRunner>(
      &menu_, views::MenuRunner::COMBOBOX | views::MenuRunner::ALWAYS_VIEWS);
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
  DCHECK((0 <= id && static_cast<size_t>(id) < accounts_.size()) ||
         id == kUseAnotherAccountCmdId);
  base::Optional<AccountInfo> account;
  if (id != kUseAnotherAccountCmdId)
    account = accounts_[id];
  std::move(account_selected_callback_).Run(account);
}
