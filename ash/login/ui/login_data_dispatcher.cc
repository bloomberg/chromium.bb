// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_data_dispatcher.h"

namespace ash {

LoginDataDispatcher::Observer::~Observer() {}

void LoginDataDispatcher::Observer::OnUsersChanged(
    const std::vector<ash::mojom::UserInfoPtr>& users) {}

void LoginDataDispatcher::Observer::OnPinEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {}

LoginDataDispatcher::LoginDataDispatcher() = default;

LoginDataDispatcher::~LoginDataDispatcher() = default;

void LoginDataDispatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LoginDataDispatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LoginDataDispatcher::NotifyUsers(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  for (auto& observer : observers_)
    observer.OnUsersChanged(users);
}

void LoginDataDispatcher::SetPinEnabledForUser(const AccountId& user,
                                               bool enabled) {
  for (auto& observer : observers_)
    observer.OnPinEnabledForUserChanged(user, enabled);
}

}  // namespace ash
