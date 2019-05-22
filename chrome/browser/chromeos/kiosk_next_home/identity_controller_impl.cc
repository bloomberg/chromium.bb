// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/identity_controller_impl.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/kiosk_next_home/metrics_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace kiosk_next_home {

IdentityControllerImpl::IdentityControllerImpl(
    mojom::IdentityControllerRequest request)
    : binding_(this, std::move(request)) {}

IdentityControllerImpl::~IdentityControllerImpl() = default;

void IdentityControllerImpl::GetUserInfo(
    mojom::IdentityController::GetUserInfoCallback callback) {
  RecordBridgeAction(BridgeAction::kGetUserInfo);
  auto user_info = mojom::UserInfo::New();
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  user_info->given_name = base::UTF16ToUTF8(user->GetGivenName());
  user_info->display_name = base::UTF16ToUTF8(user->GetDisplayName());
  std::move(callback).Run(std::move(user_info));
}

}  // namespace kiosk_next_home
}  // namespace chromeos
