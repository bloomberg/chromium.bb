// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/user_data/arc_user_data_service.h"

#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace arc {

ArcUserDataService::ArcUserDataService(
    ArcBridgeService* bridge_service,
    std::unique_ptr<BooleanPrefMember> arc_enabled_pref,
    const AccountId& account_id)
    : ArcService(bridge_service),
      arc_enabled_pref_(std::move(arc_enabled_pref)),
      primary_user_account_id_(account_id) {
  arc_bridge_service()->AddObserver(this);
  ClearIfDisabled();
}

ArcUserDataService::~ArcUserDataService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->RemoveObserver(this);
}

void ArcUserDataService::OnBridgeStopped() {
  DCHECK(thread_checker_.CalledOnValidThread());
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  if (account_id != primary_user_account_id_) {
    LOG(ERROR) << "User preferences not loaded for "
               << primary_user_account_id_.GetUserEmail()
               << ", but current primary user is " << account_id.GetUserEmail();
    primary_user_account_id_ = EmptyAccountId();
    return;
  }
  ClearIfDisabled();
}

void ArcUserDataService::ClearIfDisabled() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (arc_bridge_service()->state() != ArcBridgeService::State::STOPPED) {
    LOG(ERROR) << "ARC instance not stopped, user data can't be cleared";
    return;
  }
  if (arc_enabled_pref_->GetValue() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableArcDataWipe)) {
    return;
  }
  const cryptohome::Identification cryptohome_id(primary_user_account_id_);
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->RemoveArcData(cryptohome_id);
}

}  // namespace arc
