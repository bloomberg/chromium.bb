// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_regular.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proximity_auth/cryptauth/cryptauth_account_token_fetcher.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"

#if defined(OS_CHROMEOS)
#include "apps/app_lifetime_monitor_factory.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_reauth.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

// Key name of the local device permit record dictonary in kEasyUnlockPairing.
const char kKeyPermitAccess[] = "permitAccess";

// Key name of the remote device list in kEasyUnlockPairing.
const char kKeyDevices[] = "devices";

}  // namespace

EasyUnlockServiceRegular::EasyUnlockServiceRegular(Profile* profile)
    : EasyUnlockService(profile),
      turn_off_flow_status_(EasyUnlockService::IDLE),
      weak_ptr_factory_(this) {
}

EasyUnlockServiceRegular::~EasyUnlockServiceRegular() {
}

EasyUnlockService::Type EasyUnlockServiceRegular::GetType() const {
  return EasyUnlockService::TYPE_REGULAR;
}

std::string EasyUnlockServiceRegular::GetUserEmail() const {
  return ScreenlockBridge::GetAuthenticatedUserEmail(profile());
}

void EasyUnlockServiceRegular::LaunchSetup() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_CHROMEOS)
  // Force the user to reauthenticate by showing a modal overlay (similar to the
  // lock screen). The password obtained from the reauth is cached for a short
  // period of time and used to create the cryptohome keys for sign-in.
  if (short_lived_user_context_ && short_lived_user_context_->user_context()) {
    OpenSetupApp();
  } else {
    bool reauth_success = chromeos::EasyUnlockReauth::ReauthForUserContext(
        base::Bind(&EasyUnlockServiceRegular::OnUserContextFromReauth,
                   weak_ptr_factory_.GetWeakPtr()));
    if (!reauth_success)
      OpenSetupApp();
  }
#else
  OpenSetupApp();
#endif
}

#if defined(OS_CHROMEOS)
void EasyUnlockServiceRegular::OnUserContextFromReauth(
    const chromeos::UserContext& user_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  short_lived_user_context_.reset(new chromeos::ShortLivedUserContext(
      user_context, apps::AppLifetimeMonitorFactory::GetForProfile(profile()),
      base::ThreadTaskRunnerHandle::Get().get()));

  OpenSetupApp();
}

void EasyUnlockServiceRegular::OnKeysRefreshedForSetDevices(bool success) {
  // If the keys were refreshed successfully, the hardlock state should be
  // cleared, so Smart Lock can be used normally. Otherwise, we fall back to
  // a hardlock state to force the user to type in their credentials again.
  if (success) {
    SetHardlockStateForUser(GetUserEmail(),
                            EasyUnlockScreenlockStateHandler::NO_HARDLOCK);
  }

  // Even if the keys refresh suceeded, we still fetch the cryptohome keys as a
  // sanity check.
  CheckCryptohomeKeysAndMaybeHardlock();
}
#endif

void EasyUnlockServiceRegular::OpenSetupApp() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kEasyUnlockAppId, false);

  OpenApplication(
      AppLaunchParams(profile(), extension, extensions::LAUNCH_CONTAINER_WINDOW,
                      NEW_WINDOW, extensions::SOURCE_CHROME_INTERNAL));
}

const base::DictionaryValue* EasyUnlockServiceRegular::GetPermitAccess() const {
  const base::DictionaryValue* pairing_dict =
      profile()->GetPrefs()->GetDictionary(prefs::kEasyUnlockPairing);
  const base::DictionaryValue* permit_dict = NULL;
  if (pairing_dict &&
      pairing_dict->GetDictionary(kKeyPermitAccess, &permit_dict))
    return permit_dict;

  return NULL;
}

void EasyUnlockServiceRegular::SetPermitAccess(
    const base::DictionaryValue& permit) {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->SetWithoutPathExpansion(kKeyPermitAccess, permit.DeepCopy());
}

void EasyUnlockServiceRegular::ClearPermitAccess() {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->RemoveWithoutPathExpansion(kKeyPermitAccess, NULL);
}

const base::ListValue* EasyUnlockServiceRegular::GetRemoteDevices() const {
  const base::DictionaryValue* pairing_dict =
      profile()->GetPrefs()->GetDictionary(prefs::kEasyUnlockPairing);
  const base::ListValue* devices = NULL;
  if (pairing_dict && pairing_dict->GetList(kKeyDevices, &devices))
    return devices;

  return NULL;
}

void EasyUnlockServiceRegular::SetRemoteDevices(
    const base::ListValue& devices) {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->SetWithoutPathExpansion(kKeyDevices, devices.DeepCopy());

#if defined(OS_CHROMEOS)
  // TODO(tengs): Investigate if we can determine if the remote devices were set
  // from sync or from the setup app.
  if (short_lived_user_context_ && short_lived_user_context_->user_context() &&
      !devices.empty()) {
    // We may already have the password cached, so proceed to create the
    // cryptohome keys for sign-in or the system will be hardlocked.
    chromeos::UserSessionManager::GetInstance()->GetEasyUnlockKeyManager()
        ->RefreshKeys(
            *short_lived_user_context_->user_context(),
            devices,
            base::Bind(&EasyUnlockServiceRegular::OnKeysRefreshedForSetDevices,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    CheckCryptohomeKeysAndMaybeHardlock();
  }
#else
  CheckCryptohomeKeysAndMaybeHardlock();
#endif
}

void EasyUnlockServiceRegular::ClearRemoteDevices() {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->RemoveWithoutPathExpansion(kKeyDevices, NULL);
  CheckCryptohomeKeysAndMaybeHardlock();
}

void EasyUnlockServiceRegular::RunTurnOffFlow() {
  if (turn_off_flow_status_ == PENDING)
    return;
  DCHECK(!cryptauth_client_);

  SetTurnOffFlowStatus(PENDING);
  scoped_ptr<proximity_auth::CryptAuthAccessTokenFetcher> access_token_fetcher(
      new proximity_auth::CryptAuthAccountTokenFetcher(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()),
          SigninManagerFactory::GetForProfile(profile())
              ->GetAuthenticatedAccountId()));

  cryptauth_client_.reset(new proximity_auth::CryptAuthClient(
      access_token_fetcher.Pass(), profile()->GetRequestContext()));

  cryptauth::ToggleEasyUnlockRequest request;
  request.set_enable(false);
  request.set_apply_to_all(true);
  cryptauth_client_->ToggleEasyUnlock(
      request,
      base::Bind(&EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockServiceRegular::ResetTurnOffFlow() {
  cryptauth_client_.reset();
  SetTurnOffFlowStatus(IDLE);
}

EasyUnlockService::TurnOffFlowStatus
    EasyUnlockServiceRegular::GetTurnOffFlowStatus() const {
  return turn_off_flow_status_;
}

std::string EasyUnlockServiceRegular::GetChallenge() const {
  return std::string();
}

std::string EasyUnlockServiceRegular::GetWrappedSecret() const {
  return std::string();
}

void EasyUnlockServiceRegular::RecordEasySignInOutcome(
    const std::string& user_id,
    bool success) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::RecordPasswordLoginEvent(
    const std::string& user_id) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::InitializeInternal() {
  registrar_.Init(profile()->GetPrefs());
  registrar_.Add(
      prefs::kEasyUnlockAllowed,
      base::Bind(&EasyUnlockServiceRegular::OnPrefsChanged,
                 base::Unretained(this)));
  OnPrefsChanged();
}

void EasyUnlockServiceRegular::ShutdownInternal() {
#if defined(OS_CHROMEOS)
  short_lived_user_context_.reset();
#endif

  turn_off_flow_status_ = EasyUnlockService::IDLE;
  registrar_.RemoveAll();
}

bool EasyUnlockServiceRegular::IsAllowedInternal() {
#if defined(OS_CHROMEOS)
  if (!user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount())
    return false;

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile()))
    return false;

  if (!profile()->GetPrefs()->GetBoolean(prefs::kEasyUnlockAllowed))
    return false;

  return true;
#else
  // TODO(xiyuan): Revisit when non-chromeos platforms are supported.
  return false;
#endif
}

void EasyUnlockServiceRegular::OnPrefsChanged() {
  UpdateAppState();
}

void EasyUnlockServiceRegular::SetTurnOffFlowStatus(TurnOffFlowStatus status) {
  turn_off_flow_status_ = status;
  NotifyTurnOffOperationStatusChanged();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  cryptauth_client_.reset();

  ClearRemoteDevices();
  SetTurnOffFlowStatus(IDLE);
  ReloadApp();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed(
    const std::string& error_message) {
  LOG(WARNING) << "Failed to turn off Smart Lock: " << error_message;
  SetTurnOffFlowStatus(FAIL);
}
