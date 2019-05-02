// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/lock_to_single_user_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"

using RebootOnSignOutPolicy =
    enterprise_management::DeviceRebootOnUserSignoutProto;
using RebootOnSignOutRequest =
    cryptohome::LockToSingleUserMountUntilRebootRequest;
using RebootOnSignOutReply = cryptohome::LockToSingleUserMountUntilRebootReply;
using RebootOnSignOutResult =
    cryptohome::LockToSingleUserMountUntilRebootResult;

namespace policy {

LockToSingleUserManager::LockToSingleUserManager() {
  CHECK(chromeos::LoginState::IsInitialized());
  login_state_observer_.Add(chromeos::LoginState::Get());
}

LockToSingleUserManager::~LockToSingleUserManager() {}

void LockToSingleUserManager::LoggedInStateChanged() {
  if (!chromeos::LoginState::Get()->IsUserLoggedIn() ||
      !user_manager::UserManager::Get()->GetPrimaryUser()) {
    return;
  }
  int policy_value = -1;
  if (!chromeos::CrosSettings::Get()->GetInteger(
          chromeos::kDeviceRebootOnUserSignout, &policy_value)) {
    return;
  }
  login_state_observer_.RemoveAll();
  switch (policy_value) {
    case RebootOnSignOutPolicy::ALWAYS:
      LockToSingleUser();
      break;
    case RebootOnSignOutPolicy::ARC_SESSION:
      arc_session_observer_.Add(arc::ArcSessionManager::Get());
      break;
  }
}

void LockToSingleUserManager::OnArcStarted() {
  arc_session_observer_.RemoveAll();
  LockToSingleUser();
}

void LockToSingleUserManager::LockToSingleUser() {
  cryptohome::AccountIdentifier account_id =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  RebootOnSignOutRequest request;
  request.mutable_account_id()->CopyFrom(account_id);
  chromeos::CryptohomeClient::Get()->LockToSingleUserMountUntilReboot(
      request,
      base::BindOnce(
          &LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone,
          weak_factory_.GetWeakPtr()));
}

void LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone(
    base::Optional<cryptohome::BaseReply> reply) {
  // TODO(igorcov): Add UMA stats and remove multi user sign in option from UI.
  if (!reply || !reply->HasExtension(RebootOnSignOutReply::reply)) {
    LOG(ERROR) << "Signing out user: no reply from "
                  "LockToSingleUserMountUntilReboot D-Bus call.";
    chrome::AttemptUserExit();
    return;
  }

  // Force user logout if failed to lock the device to single user mount.
  const RebootOnSignOutReply extension =
      reply->GetExtension(RebootOnSignOutReply::reply);
  if (extension.result() == RebootOnSignOutResult::SUCCESS ||
      extension.result() == RebootOnSignOutResult::PCR_ALREADY_EXTENDED) {
    // The device is locked to single user on TPM level. Update the cache in
    // SessionTerminationManager, so that it triggers reboot on sign out.
    chromeos::SessionTerminationManager::Get()->SetDeviceLockedToSingleUser();
  } else {
    LOG(ERROR) << "Signing out user: failed to lock device to single user: "
               << extension.result();
    chrome::AttemptUserExit();
  }
}

}  // namespace policy
