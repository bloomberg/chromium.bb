// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller_observer.h"

#include "ash/login/login_screen_controller.h"

namespace ash {

LoginScreenControllerObserver::~LoginScreenControllerObserver() = default;

void LoginScreenControllerObserver::SetAvatarForUser(
    const AccountId& account_id,
    const mojom::UserAvatarPtr& avatar) {}

void LoginScreenControllerObserver::OnFocusLeavingLockScreenApps(bool reverse) {
}

void LoginScreenControllerObserver::OnOobeDialogStateChanged(
    mojom::OobeDialogState state) {}

}  // namespace ash
