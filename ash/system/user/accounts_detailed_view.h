// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_ACCOUNTS_DETAILED_VIEW_H_
#define ASH_SYSTEM_USER_ACCOUNTS_DETAILED_VIEW_H_

#include <map>
#include <string>

#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/user_accounts_delegate.h"
#include "ash/system/user/user_view.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace ash {

class TrayUser;

namespace tray {

// This detailed view appears after a click on the primary user's card when the
// new account managment is enabled.
class AccountsDetailedView : public TrayDetailsView,
                             public ViewClickListener,
                             public views::ButtonListener,
                             public ash::tray::UserAccountsDelegate::Observer {
 public:
  AccountsDetailedView(TrayUser* owner, user::LoginStatus login_status);
  virtual ~AccountsDetailedView();

 private:
  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ash::tray::UserAccountsDelegate::Observer.
  virtual void AccountListChanged() OVERRIDE;

  void AddHeader(user::LoginStatus login_status);
  void AddAccountList();
  void AddAddAccountButton();
  void AddFooter();

  void UpdateAccountList();

  views::View* CreateDeleteButton();

  ash::tray::UserAccountsDelegate* delegate_;
  views::View* account_list_;
  views::View* add_account_button_;
  views::View* add_user_button_;
  std::map<views::View*, std::string> delete_button_to_account_id_;

  DISALLOW_COPY_AND_ASSIGN(AccountsDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_ACCOUNTS_DETAILED_VIEW_H_
