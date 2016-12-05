// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_user_flow.h"

#include "base/bind.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/signin/core/account_id/account_id.h"

namespace chromeos {

BootstrapUserFlow::BootstrapUserFlow(const UserContext& user_context,
                                     bool is_new_account)
    : ExtendedUserFlow(user_context.GetAccountId()),
      user_context_(user_context),
      is_new_account_(is_new_account),
      finished_(false),
      user_profile_(nullptr),
      weak_ptr_factory_(this) {
  ChromeUserManager::Get()->GetBootstrapManager()->AddPendingBootstrap(
      user_context_.GetAccountId());
}

BootstrapUserFlow::~BootstrapUserFlow() {
}

void BootstrapUserFlow::StartAutoPairing() {
  DCHECK(user_profile_);

  VLOG(2) << "BootstrapUserFlow StartAutoPairing";

  EasyUnlockService* service = EasyUnlockService::Get(user_profile_);
  if (!service->IsAllowed()) {
    // TODO(xiyuan): Show error UI.
    LOG(ERROR) << "Bootstrapped failed because EasyUnlock service not allowed.";
    chrome::AttemptUserExit();
    return;
  }

  service->StartAutoPairing(base::Bind(&BootstrapUserFlow::SetAutoPairingResult,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void BootstrapUserFlow::SetAutoPairingResult(bool success,
                                             const std::string& error_message) {
  VLOG(2) << "BootstrapUserFlow::SetAutoPairingResult"
          << ", success=" << success;

  if (!success) {
    // TODO(xiyuan): Show error UI.
    LOG(ERROR) << "Easy bootstrap auto pairing failed, err=" << error_message;
    chrome::AttemptUserExit();
    return;
  }

  // TODO(xiyuan): Remove this and properly mock EasyUnlockKeyManager.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    Finish();
    return;
  }

  chromeos::UserSessionManager::GetInstance()
      ->GetEasyUnlockKeyManager()
      ->RefreshKeys(
          user_context_,
          *EasyUnlockService::Get(user_profile_)->GetRemoteDevices(),
          base::Bind(&BootstrapUserFlow::OnKeysRefreshed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BootstrapUserFlow::OnKeysRefreshed(bool success) {
  if (!success) {
    // TODO(xiyuan): Show error UI.
    LOG(ERROR) << "Easy bootstrap failed to refresh cryptohome keys.";
    chrome::AttemptUserExit();
    return;
  }

  EasyUnlockService::Get(user_profile_)
      ->SetHardlockState(EasyUnlockScreenlockStateHandler::NO_HARDLOCK);
  RemoveBootstrapRandomKey();
}

void BootstrapUserFlow::RemoveBootstrapRandomKey() {
  scoped_refptr<ExtendedAuthenticator> authenticator(
      ExtendedAuthenticator::Create(this));

  const char kLegacyKeyLabel[] = "legacy-0";
  authenticator->RemoveKey(
      user_context_,
      kLegacyKeyLabel,
      base::Bind(&BootstrapUserFlow::OnBootstrapRandomKeyRemoved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BootstrapUserFlow::OnBootstrapRandomKeyRemoved() {
  Finish();
}

void BootstrapUserFlow::Finish() {
  VLOG(2) << "BootstrapUserFlow::Finish";
  finished_ = true;

  ChromeUserManager::Get()->GetBootstrapManager()->FinishPendingBootstrap(
      user_context_.GetAccountId());
  UserSessionManager::GetInstance()->DoBrowserLaunch(user_profile_, host());

  user_profile_ = nullptr;
  UnregisterFlowSoon();
}

bool BootstrapUserFlow::CanLockScreen() {
  return false;
}

bool BootstrapUserFlow::CanStartArc() {
  return false;
}

bool BootstrapUserFlow::ShouldLaunchBrowser() {
  return finished_;
}

bool BootstrapUserFlow::ShouldSkipPostLoginScreens() {
  return true;
}

bool BootstrapUserFlow::HandleLoginFailure(
    const chromeos::AuthFailure& failure) {
  NOTREACHED();
  return false;
}

void BootstrapUserFlow::HandleLoginSuccess(
    const chromeos::UserContext& context) {
}

bool BootstrapUserFlow::HandlePasswordChangeDetected() {
  NOTREACHED();
  return false;
}

void BootstrapUserFlow::HandleOAuthTokenStatusChange(
    user_manager::User::OAuthTokenStatus status) {
  if (status == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID) {
    // TODO(xiyuan): Show error UI.
    LOG(ERROR) << "Bootstrapped failed because of bad refresh token.";
    chrome::AttemptUserExit();
    return;
  }
}

void BootstrapUserFlow::LaunchExtraSteps(Profile* user_profile) {
  user_profile_ = user_profile;

  // Auto pairing only when a random key is used to boostrap cryptohome.
  if (is_new_account_) {
    StartAutoPairing();
    return;
  }

  Finish();
}

bool BootstrapUserFlow::SupportsEarlyRestartToApplyFlags() {
  return false;
}

void BootstrapUserFlow::OnAuthenticationFailure(
    ExtendedAuthenticator::AuthState state) {
  // TODO(xiyuan): Show error UI.
  LOG(ERROR) << "Bootstrapped failed because authenticator falure"
             << ", state=" << state;
  chrome::AttemptUserExit();
}

}  // namespace chromeos
