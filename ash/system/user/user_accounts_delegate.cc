// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/user_accounts_delegate.h"

namespace ash {
namespace tray {

UserAccountsDelegate::UserAccountsDelegate() {}

UserAccountsDelegate::~UserAccountsDelegate() {}

void UserAccountsDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserAccountsDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserAccountsDelegate::NotifyAccountListChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, AccountListChanged());
}

}  // namespace tray
}  // namespace ash
