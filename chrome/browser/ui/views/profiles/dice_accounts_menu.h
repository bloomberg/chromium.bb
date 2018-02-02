// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_ACCOUNTS_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_ACCOUNTS_MENU_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/signin/core/browser/account_info.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace views {
class View;
}  // namespace views

// Menu allowing the user to select an account for enabling Sync when desktop
// identity consistency (aka DICE) is enabled.
class DiceAccountsMenu : public ui::SimpleMenuModel::Delegate {
 public:
  // Callback to enable Sync for |account| if available. |account| corresponds
  // to the account that was selected or |nullopt| if the "Use another account"
  // button was selected.
  using Callback =
      base::OnceCallback<void(const base::Optional<AccountInfo>& account)>;

  // Builds the accounts menu. Each account from |accounts| is placed in a menu
  // item showing the email and the corresponding icon from |icons|. The last
  // item in the accounts menu is the "Use another accounts" button. Separators
  // are added at the top, bottom and between each item to increase the spacing.
  DiceAccountsMenu(const std::vector<AccountInfo>& accounts,
                   const std::vector<gfx::Image>& icons,
                   Callback account_selected_callback);
  ~DiceAccountsMenu() override;

  // Shows the accounts menu below |anchor_view|. This method can only be called
  // once.
  void Show(views::View* anchor_view);

 private:
  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  ui::SimpleMenuModel menu_;
  std::unique_ptr<views::MenuRunner> runner_;

  std::vector<AccountInfo> accounts_;

  Callback account_selected_callback_;

  DISALLOW_COPY_AND_ASSIGN(DiceAccountsMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_ACCOUNTS_MENU_H_
