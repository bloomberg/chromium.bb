// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_USER_ACCOUNTS_DELEGATE_H_
#define ASH_SYSTEM_USER_USER_ACCOUNTS_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {
namespace tray {

class ASH_EXPORT UserAccountsDelegate {
 public:
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // Called when the account list of user has been changed.
    virtual void AccountListChanged() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  UserAccountsDelegate();
  virtual ~UserAccountsDelegate();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the user's primary account's ID.
  virtual std::string GetPrimaryAccountId() = 0;

  // Returns a list of the user's secondary accounts' IDs.
  virtual std::vector<std::string> GetSecondaryAccountIds() = 0;

  // Returns display name for given |account_id|.
  virtual std::string GetAccountDisplayName(const std::string& account_id) = 0;

  // Deletes given |account_id| from the list of user's account. Passing
  // |account_id| that is not from list is no-op.
  virtual void DeleteAccount(const std::string& account_id) = 0;

  // Launches a dialog which lets the user add a new account.
  virtual void LaunchAddAccountDialog() = 0;

 protected:
  void NotifyAccountListChanged();

 private:
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(UserAccountsDelegate);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_USER_ACCOUNTS_DELEGATE_H_
