// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_STUB_USER_ACCOUNTS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_STUB_USER_ACCOUNTS_DELEGATE_H_

#include "ash/system/user/user_accounts_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

class StubUserAccountsDelegate : public ash::tray::UserAccountsDelegate {
 public:
  explicit StubUserAccountsDelegate(const std::string& owner_id);
  virtual ~StubUserAccountsDelegate();

  void AddAccount(const std::string& account);

  // Overridden from ash::tray::UserAccountsDelegate:
  virtual std::string GetPrimaryAccountId() OVERRIDE;
  virtual std::vector<std::string> GetSecondaryAccountIds() OVERRIDE;
  virtual std::string GetAccountDisplayName(const std::string& account_id)
      OVERRIDE;
  virtual void DeleteAccount(const std::string& account_id) OVERRIDE;
  virtual void LaunchAddAccountDialog() OVERRIDE;

 private:
  std::string primary_account_;
  std::vector<std::string> secondary_accounts_;

  DISALLOW_COPY_AND_ASSIGN(StubUserAccountsDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_STUB_USER_ACCOUNTS_DELEGATE_H_
