// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_regular.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/user_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proximity_auth/cryptauth/cryptauth_account_token_fetcher.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
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
      will_unlock_using_easy_unlock_(false),
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

  // Use this opportunity to clear the crytohome keys if it was not already
  // cleared earlier.
  const base::ListValue* devices = GetRemoteDevices();
  if (!devices || devices->empty()) {
    chromeos::EasyUnlockKeyManager* key_manager =
        chromeos::UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
    key_manager->RefreshKeys(
        user_context, base::ListValue(),
        base::Bind(&EasyUnlockServiceRegular::SetHardlockAfterKeyOperation,
                   weak_ptr_factory_.GetWeakPtr(),
                   EasyUnlockScreenlockStateHandler::NO_PAIRING));
  }
}

void EasyUnlockServiceRegular::SetHardlockAfterKeyOperation(
    EasyUnlockScreenlockStateHandler::HardlockState state_on_success,
    bool success) {
  if (success)
    SetHardlockStateForUser(GetUserEmail(), state_on_success);

  // Even if the refresh keys operation suceeded, we still fetch and check the
  // cryptohome keys against the keys in local preferences as a sanity check.
  CheckCryptohomeKeysAndMaybeHardlock();
}
#endif

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
  if (devices.empty())
    pairing_update->RemoveWithoutPathExpansion(kKeyDevices, NULL);
  else
    pairing_update->SetWithoutPathExpansion(kKeyDevices, devices.DeepCopy());

#if defined(OS_CHROMEOS)
  // TODO(tengs): Investigate if we can determine if the remote devices were set
  // from sync or from the setup app.
  if (short_lived_user_context_ && short_lived_user_context_->user_context()) {
    // We may already have the password cached, so proceed to create the
    // cryptohome keys for sign-in or the system will be hardlocked.
    chromeos::UserSessionManager::GetInstance()
        ->GetEasyUnlockKeyManager()
        ->RefreshKeys(
            *short_lived_user_context_->user_context(), devices,
            base::Bind(&EasyUnlockServiceRegular::SetHardlockAfterKeyOperation,
                       weak_ptr_factory_.GetWeakPtr(),
                       EasyUnlockScreenlockStateHandler::NO_HARDLOCK));
  } else {
    CheckCryptohomeKeysAndMaybeHardlock();
  }
#else
  CheckCryptohomeKeysAndMaybeHardlock();
#endif
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

void EasyUnlockServiceRegular::StartAutoPairing(
    const AutoPairingResultCallback& callback) {
  if (!auto_pairing_callback_.is_null()) {
    LOG(ERROR)
        << "Start auto pairing when there is another auto pairing requested.";
    callback.Run(false, std::string());
    return;
  }

  auto_pairing_callback_ = callback;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::easy_unlock_private::OnStartAutoPairing::kEventName,
      args.Pass()));
  extensions::EventRouter::Get(profile())->DispatchEventWithLazyListener(
       extension_misc::kEasyUnlockAppId, event.Pass());
}

void EasyUnlockServiceRegular::SetAutoPairingResult(
    bool success,
    const std::string& error) {
  DCHECK(!auto_pairing_callback_.is_null());

  auto_pairing_callback_.Run(success, error);
  auto_pairing_callback_.Reset();
}

void EasyUnlockServiceRegular::InitializeInternal() {
  ScreenlockBridge::Get()->AddObserver(this);
  registrar_.Init(profile()->GetPrefs());
  registrar_.Add(
      prefs::kEasyUnlockAllowed,
      base::Bind(&EasyUnlockServiceRegular::OnPrefsChanged,
                 base::Unretained(this)));
  registrar_.Add(prefs::kEasyUnlockProximityRequired,
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
  ScreenlockBridge::Get()->RemoveObserver(this);
}

bool EasyUnlockServiceRegular::IsAllowedInternal() const {
#if defined(OS_CHROMEOS)
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsUserWithGaiaAccount())
    return false;

  // TODO(tengs): Ephemeral accounts generate a new enrollment every time they
  // are added, so disable Smart Lock to reduce enrollments on server. However,
  // ephemeral accounts can be locked, so we should revisit this use case.
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral())
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

void EasyUnlockServiceRegular::OnWillFinalizeUnlock(bool success) {
  will_unlock_using_easy_unlock_ = success;
}

void EasyUnlockServiceRegular::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  will_unlock_using_easy_unlock_ = false;
}

void EasyUnlockServiceRegular::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // Notifications of signin screen unlock events can also reach this code path;
  // disregard them.
  if (screen_type != ScreenlockBridge::LockHandler::LOCK_SCREEN)
    return;

  // Only record metrics for users who have enabled the feature.
  if (IsEnabled()) {
    EasyUnlockAuthEvent event =
        will_unlock_using_easy_unlock_
            ? EASY_UNLOCK_SUCCESS
            : GetPasswordAuthEvent();
    RecordEasyUnlockScreenUnlockEvent(event);
  }

  will_unlock_using_easy_unlock_ = false;
}

void EasyUnlockServiceRegular::OnFocusedUserChanged(
    const std::string& user_id) {
  // Nothing to do.
}

void EasyUnlockServiceRegular::OnPrefsChanged() {
  SyncProfilePrefsToLocalState();
  UpdateAppState();
}

void EasyUnlockServiceRegular::SetTurnOffFlowStatus(TurnOffFlowStatus status) {
  turn_off_flow_status_ = status;
  NotifyTurnOffOperationStatusChanged();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  cryptauth_client_.reset();

  SetRemoteDevices(base::ListValue());
  SetTurnOffFlowStatus(IDLE);
  ReloadAppAndLockScreen();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed(
    const std::string& error_message) {
  LOG(WARNING) << "Failed to turn off Smart Lock: " << error_message;
  SetTurnOffFlowStatus(FAIL);
}

void EasyUnlockServiceRegular::SyncProfilePrefsToLocalState() {
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : NULL;
  PrefService* profile_prefs = profile()->GetPrefs();
  if (!local_state || !profile_prefs)
    return;

  // Create the dictionary of Easy Unlock preferences for the current user. The
  // items in the dictionary are the same profile prefs used for Easy Unlock.
  scoped_ptr<base::DictionaryValue> user_prefs_dict(
      new base::DictionaryValue());
  user_prefs_dict->SetBooleanWithoutPathExpansion(
      prefs::kEasyUnlockProximityRequired,
      profile_prefs->GetBoolean(prefs::kEasyUnlockProximityRequired));

  DictionaryPrefUpdate update(local_state,
                              prefs::kEasyUnlockLocalStateUserPrefs);
  std::string user_email = GetUserEmail();
  update->SetWithoutPathExpansion(user_email, user_prefs_dict.Pass());
}
