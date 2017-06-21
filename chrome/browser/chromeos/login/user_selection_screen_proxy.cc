// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_selection_screen_proxy.h"

#include <utility>

#include "chrome/browser/ui/ash/lock_screen_client.h"

namespace chromeos {

namespace {

// Converts parameters to a mojo struct that can be sent to the
// screenlock view-based UI.
ash::mojom::UserPodCustomIconOptionsPtr ToUserPodCustomIconOptionsPtr(
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  ash::mojom::UserPodCustomIconOptionsPtr icon =
      ash::mojom::UserPodCustomIconOptions::New();
  icon->id = icon_options.GetIDString();

  if (!icon_options.tooltip().empty()) {
    icon->tooltip = icon_options.tooltip();
    icon->autoshow_tooltip = icon_options.autoshow_tooltip();
  }

  if (!icon_options.aria_label().empty())
    icon->aria_label = icon_options.aria_label();

  if (icon_options.hardlock_on_click())
    icon->hardlock_on_click = true;

  if (icon_options.is_trial_run())
    icon->is_trial_run = true;

  return icon;
}

}  // namespace

UserSelectionScreenProxy::UserSelectionScreenProxy() = default;

UserSelectionScreenProxy::~UserSelectionScreenProxy() = default;

void UserSelectionScreenProxy::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  ash::mojom::UserPodCustomIconOptionsPtr icon =
      ToUserPodCustomIconOptionsPtr(icon_options);
  if (!icon)
    return;
  LockScreenClient::Get()->ShowUserPodCustomIcon(account_id, std::move(icon));
}

void UserSelectionScreenProxy::HideUserPodCustomIcon(
    const AccountId& account_id) {
  LockScreenClient::Get()->HideUserPodCustomIcon(account_id);
}

void UserSelectionScreenProxy::SetAuthType(
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const base::string16& initial_value) {
  LockScreenClient::Get()->SetAuthType(account_id, auth_type, initial_value);
}

base::WeakPtr<chromeos::UserBoardView> UserSelectionScreenProxy::GetWeakPtr() {
  return base::WeakPtr<chromeos::UserBoardView>();
}

}  // namespace chromeos
